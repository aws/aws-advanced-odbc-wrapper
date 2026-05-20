// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "multi_az_topology_util.h"

#include <algorithm>

#include "../util/odbc_helper.h"
#include "../util/plugin_service.h"

MultiAzTopologyUtil::MultiAzTopologyUtil(const std::shared_ptr<OdbcHelper>& odbc_helper, const std::shared_ptr<Dialect>& dialect) : TopologyUtil(odbc_helper, dialect) {}

std::string MultiAzTopologyUtil::GetWriterId(SQLHDBC hdbc) {
    const DBC* dbc = static_cast<DBC*>(hdbc);
    if (!dbc || !dbc->wrapped_dbc) {
        return "";
    }

    SQLHSTMT stmt = SQL_NULL_HANDLE;
    RdsLibResult res = this->odbc_helper_->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);
    if (!SQL_SUCCEEDED(res.fn_result)) {
        LOG(ERROR) << "Failed to allocate statement";
        return "";
    }

    res = this->odbc_helper_->ExecDirect(&stmt, dialect_->GetReplicaSourceQuery());

    if (!SQL_SUCCEEDED(res.fn_result)) {
        this->odbc_helper_->BaseFreeStmt(&stmt);
        LOG(ERROR) << "Failed to query writer ID";
        return "";
    }

    SQLTCHAR writer_id_buf[BUFFER_SIZE * 2] = {0};
    SQLLEN writer_id_len = 0;

    const RdsLibResult fetch_res = this->odbc_helper_->Fetch(&stmt);

    std::string writer_id;

    if (SQL_SUCCEEDED(fetch_res.fn_result)) {
        // Returned something -> connected to a reader.
        SQLSMALLINT col_count = 0;
        NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLNumResultCols, RDS_STR_SQLNumResultCols,
            stmt, &col_count
        );
        SQLSMALLINT writer_col = 0;
        std::string target_col = dialect_->GetWriterIdColumnName();
        std::ranges::transform(target_col, target_col.begin(), ::tolower);

        for (SQLSMALLINT i = 1; i <= col_count; i++) {
            SQLTCHAR col_name[BUFFER_SIZE] = {0};
            SQLSMALLINT name_len = 0;
            NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLDescribeCol, RDS_STR_SQLDescribeCol,
                stmt, i, col_name, BUFFER_SIZE, &name_len, nullptr, nullptr, nullptr, nullptr
            );
#if UNICODE
            Convert4To2ByteString(this->odbc_helper_->GetUse4BytesBaseDriver(), col_name, nullptr, BUFFER_SIZE);
#endif
            std::string col_name_str = AS_UTF8_CSTR(col_name);
            std::ranges::transform(col_name_str, col_name_str.begin(), ::tolower);
            if (col_name_str == target_col) {
                writer_col = i;
                break;
            }
        }
        if (writer_col != 0) {
            NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLGetData, RDS_STR_SQLGetData,
                stmt, writer_col, SQL_C_TCHAR, writer_id_buf, BUFFER_SIZE, &writer_id_len
            );
        }
#if UNICODE
        Convert4To2ByteString(this->odbc_helper_->GetUse4BytesBaseDriver(), writer_id_buf, nullptr, BUFFER_SIZE);
#endif
        writer_id = AS_UTF8_CSTR(writer_id_buf);
    } else {
        // Returned nothing -> connected to the writer.
        // Close the current statement and run the node ID query instead.
        this->odbc_helper_->BaseFreeStmt(&stmt);
        stmt = SQL_NULL_HANDLE;
        res = this->odbc_helper_->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);
        if (!SQL_SUCCEEDED(res.fn_result)) {
            LOG(ERROR) << "Failed to allocate statement";
            return "";
        }
        res = this->odbc_helper_->ExecDirect(&stmt, dialect_->GetNodeIdQuery());
        if (SQL_SUCCEEDED(res.fn_result)) {
            SQLTCHAR id_buf[BUFFER_SIZE * 2] = {0};
            SQLLEN id_len = 0;
            this->odbc_helper_->BindCol(&stmt, 1, SQL_C_TCHAR, &id_buf, BUFFER_SIZE, &id_len);
            const RdsLibResult fetch_res2 = this->odbc_helper_->Fetch(&stmt);
            if (SQL_SUCCEEDED(fetch_res2.fn_result)) {
#if UNICODE
                Convert4To2ByteString(this->odbc_helper_->GetUse4BytesBaseDriver(), id_buf, nullptr, BUFFER_SIZE);
#endif
                writer_id = AS_UTF8_CSTR(id_buf);
            }
        }
    }
    this->odbc_helper_->BaseFreeStmt(&stmt);
    return writer_id;
}

std::vector<HostInfo> MultiAzTopologyUtil::GetHosts(SQLHDBC hdbc, const HostInfo& /*initial_host*/, const HostInfo& host_template) {
    const DBC* dbc = static_cast<DBC*>(hdbc);
    if (!dbc || !dbc->wrapped_dbc) {
        return {};
    }

    const std::string writer_id = GetWriterId(hdbc);

    SQLHSTMT stmt = SQL_NULL_HANDLE;
    RdsLibResult res = this->odbc_helper_->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);
    if (!SQL_SUCCEEDED(res.fn_result)) {
        return {};
    }

    res = this->odbc_helper_->ExecDirect(&stmt, dialect_->GetTopologyQuery());
    if (!SQL_SUCCEEDED(res.fn_result)) {
        this->odbc_helper_->BaseFreeStmt(&stmt);
        return {};
    }

    SQLTCHAR node_id[BUFFER_SIZE * 2] = {0};
    SQLTCHAR instance_id[BUFFER_SIZE * 2] = {0};
    SQLLEN node_id_len = 0;
    SQLLEN instance_id_len = 0;

    this->odbc_helper_->BindCol(&stmt, 1, SQL_C_TCHAR, &node_id, BUFFER_SIZE, &node_id_len);
    this->odbc_helper_->BindCol(&stmt, 2, SQL_C_TCHAR, &instance_id, BUFFER_SIZE, &instance_id_len);

    std::vector<HostInfo> hosts;
    res = this->odbc_helper_->Fetch(&stmt);
    while (SQL_SUCCEEDED(res.fn_result)) {
#if UNICODE
        Convert4To2ByteString(this->odbc_helper_->GetUse4BytesBaseDriver(), node_id, nullptr, BUFFER_SIZE);
        Convert4To2ByteString(this->odbc_helper_->GetUse4BytesBaseDriver(), instance_id, nullptr, BUFFER_SIZE);
#endif
        const std::string current_node_id = AS_UTF8_CSTR(node_id);
        const HOST_ROLE role = (current_node_id == writer_id) ? WRITER : READER;
        const HostInfo new_host = CreateHost(instance_id, role, host_template);
        hosts.push_back(new_host);
        res = this->odbc_helper_->Fetch(&stmt);
    }

    this->odbc_helper_->BaseFreeStmt(&stmt);
    return hosts;
}

HostInfo MultiAzTopologyUtil::CreateHost(
    SQLTCHAR* endpoint,
    const HOST_ROLE role,
    const HostInfo& host_template)
{
    const std::string full_endpoint = AS_UTF8_CSTR(endpoint);
    return TopologyUtil::CreateHost(full_endpoint, host_template.GetPort(), UP, role, 0, std::chrono::steady_clock::now());
}

