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

#include "topology_util.h"

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#include <cmath>

#include "../driver.h"
#include "../odbcapi.h"
#include "../util/logger_wrapper.h"
#include "../util/odbc_helper.h"
#include "../util/rds_strings.h"
#include "aurora_topology_util.h"

TopologyUtil::TopologyUtil(const std::shared_ptr<OdbcHelper> &odbc_helper, const std::shared_ptr<Dialect> &dialect)
    : odbc_helper_{ odbc_helper },
      dialect_{ dialect } {}

std::string TopologyUtil::GetWriterId(SQLHDBC hdbc)
{
    SQLRETURN rc;
    SQLHSTMT stmt = SQL_NULL_HANDLE;
#if UNICODE
    SQLTCHAR writer_id[BUFFER_SIZE*2] = {0};
    SQLTCHAR writer_id_final[BUFFER_SIZE] = {0};
#else
    SQLTCHAR writer_id[BUFFER_SIZE] = {0};
#endif
    SQLLEN len = 0;
    RdsLibResult res;
    const DBC* dbc = static_cast<DBC*>(hdbc);

    if (!dbc || !dbc->wrapped_dbc) {
        LOG(ERROR) << "Topology Query passed in null DBC";
        return "";
    }

    res = this->odbc_helper_->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);

    if (SQL_SUCCEEDED(res.fn_result)) {
        res = this->odbc_helper_->ExecDirect(&stmt, dialect_->GetWriterIdQuery(), dbc->env->use_4_bytes);

        if (SQL_SUCCEEDED(res.fn_result)) {
            this->odbc_helper_->BindCol(&stmt, 1, SQL_C_TCHAR, &writer_id, BUFFER_SIZE, &len);
            this->odbc_helper_->Fetch(&stmt);
        }
        this->odbc_helper_->BaseFreeStmt(&stmt);
    }

#if UNICODE
    Convert4To2ByteString(dbc->env->use_4_bytes, writer_id, writer_id_final, BUFFER_SIZE);
    return AS_UTF8_CSTR(writer_id_final);
#endif

    return AS_UTF8_CSTR(writer_id);
}

std::string TopologyUtil::GetInstanceId(SQLHDBC hdbc) {
    SQLRETURN rc;
    SQLHSTMT stmt = SQL_NULL_HANDLE;
#if UNICODE
    SQLTCHAR instance_id[BUFFER_SIZE*2] = {0};
    SQLTCHAR instance_id_final[BUFFER_SIZE] = {0};
#else
    SQLTCHAR instance_id[BUFFER_SIZE] = {0};
#endif
    SQLLEN len = 0;
    RdsLibResult res;
    const DBC* dbc = static_cast<DBC*>(hdbc);

    if (!dbc || !dbc->wrapped_dbc) {
        LOG(ERROR) << "Topology Query passed in null DBC";
        return "";
    }

    res = this->odbc_helper_->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);

    if (SQL_SUCCEEDED(res.fn_result)) {
        res = this->odbc_helper_->ExecDirect(&stmt, dialect_->GetNodeIdQuery(), dbc->env->use_4_bytes);

        if (SQL_SUCCEEDED(res.fn_result)) {
            this->odbc_helper_->BindCol(&stmt, 1, SQL_C_TCHAR, &instance_id, BUFFER_SIZE, &len);
            this->odbc_helper_->Fetch(&stmt);
        }
        this->odbc_helper_->BaseFreeStmt(&stmt);
    }

#if UNICODE
    Convert4To2ByteString(dbc->env->use_4_bytes, instance_id, instance_id_final, BUFFER_SIZE);
    return AS_UTF8_CSTR(instance_id_final);
#endif

    return AS_UTF8_CSTR(instance_id);
}

std::vector<HostInfo> TopologyUtil::QueryTopology(SQLHDBC hdbc, const HostInfo& initial_host, const HostInfo& host_template)
{
    SQLHSTMT stmt = SQL_NULL_HANDLE;
    std::vector<HostInfo> hosts;
    const DBC* dbc = static_cast<DBC*>(hdbc);

    if (!dbc || !dbc->wrapped_dbc) {
        LOG(ERROR) << "Topology Query passed in null DBC";
        return hosts;
    }

    RdsLibResult res = this->odbc_helper_->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);

    if (SQL_SUCCEEDED(res.fn_result)) {
        res = this->odbc_helper_->ExecDirect(&stmt, dialect_->GetTopologyQuery(), dbc->env->use_4_bytes);
        if (SQL_SUCCEEDED(res.fn_result)) {
            hosts = GetHosts(stmt, initial_host, host_template, dbc->env->use_4_bytes);
        }
        this->odbc_helper_->BaseFreeStmt(&stmt);
    }

    LOG_IF(WARNING, hosts.empty()) << "Failed to fetch any instances from topology";
    return VerifyWriter(hosts);
}

std::vector<HostInfo> TopologyUtil::VerifyWriter(const std::vector<HostInfo>& all_hosts)
{
    std::vector<HostInfo> hosts;
    hosts.reserve(all_hosts.size());

    const HostInfo* newest_writer = nullptr;

    for (const HostInfo& host : all_hosts) {
        if (host.IsHostWriter()) {
            if (newest_writer == nullptr||
                newest_writer->GetLastUpdate() < host.GetLastUpdate()) {
                newest_writer = &host;
            }
        } else {
            hosts.push_back(host);
        }
    }

    if (!newest_writer) {
        LOG(WARNING) << "No writers found within list of hosts.";
        return {};
    }
    hosts.push_back(*newest_writer);

    return hosts;
}

HOST_ROLE TopologyUtil::GetConnectionRole(SQLHDBC hdbc) {
    SQLRETURN rc;
    SQLHSTMT stmt = SQL_NULL_HANDLE;
    bool is_reader = false;
    SQLLEN len = 0;
    RdsLibResult res;
    const DBC* dbc = static_cast<DBC*>(hdbc);

    if (!dbc || !dbc->wrapped_dbc) {
        LOG(ERROR) << "Topology Query passed in null DBC";
        return HOST_ROLE::UNKNOWN;
    }

    res = this->odbc_helper_->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);

    if (SQL_SUCCEEDED(res.fn_result)) {
        this->odbc_helper_->ExecDirect(&stmt, dialect_->GetIsReaderQuery(), dbc->env->use_4_bytes);

        this->odbc_helper_->BindCol(&stmt, IS_READER_COL, SQL_BIT, &is_reader, sizeof(is_reader), &len);
        this->odbc_helper_->Fetch(&stmt);
        this->odbc_helper_->BaseFreeStmt(&stmt);
    }

    return is_reader ? HOST_ROLE::READER : HOST_ROLE::WRITER;
}

HostInfo TopologyUtil::CreateHost(
    std::string host,
    int port,
    HOST_STATE state,
    HOST_ROLE role,
    uint64_t weight,
    std::chrono::steady_clock::time_point last_update)
{
    return {std::move(host), port, state, role, weight, last_update};
}
