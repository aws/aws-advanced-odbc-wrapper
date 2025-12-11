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

#include "cluster_topology_query_helper.h"

#include <iostream>
#include <sql.h>
#include <sqlext.h>

#include <cmath>

#include "../../driver.h"
#include "../../odbcapi.h"
#include "../../util/logger_wrapper.h"
#include "../../util/odbc_helper.h"
#include "../../util/rds_strings.h"

ClusterTopologyQueryHelper::ClusterTopologyQueryHelper(
    const std::shared_ptr<RdsLibLoader> &lib_loader,
    const int port,
    std::string endpoint_template,
    std::string topology_query,
    std::string writer_id_query,
    std::string node_id_query)
    : lib_loader_{ lib_loader },
      port{ port },
      endpoint_template_{ std::move(endpoint_template) },
      topology_query_{ std::move(topology_query) },
      writer_id_query_{ std::move(writer_id_query) },
      node_id_query_{ std::move(node_id_query) } {}

std::string ClusterTopologyQueryHelper::GetWriterId(SQLHDBC hdbc, const std::shared_ptr<OdbcHelper>& odbc_helper)
{
    SQLRETURN rc;
    SQLHSTMT stmt = SQL_NULL_HANDLE;
    SQLTCHAR writer_id[BUFFER_SIZE] = {0};
    const SQLLEN rt = 0;
    RdsLibResult res;
    const DBC* dbc = static_cast<DBC*>(hdbc);

    if (!dbc || !dbc->wrapped_dbc) {
        LOG(ERROR) << "Topology Query passed in null DBC";
        return "";
    }

    res = odbc_helper->AllocStmt(dbc->wrapped_dbc, lib_loader_, stmt);

    if (SQL_SUCCEEDED(res.fn_result)) {
        odbc_helper->SQLExecDirect(lib_loader_, stmt, writer_id_query_);

        odbc_helper->SQLBindCol(lib_loader_, stmt, NODE_ID_COL, SQL_C_TCHAR, &writer_id, sizeof(writer_id), rt);
        odbc_helper->SQLFetch(lib_loader_, stmt);
        odbc_helper->FreeStmt(lib_loader_, stmt);
    }

    return AS_UTF8_CSTR(writer_id);
}

std::vector<HostInfo> ClusterTopologyQueryHelper::QueryTopology(SQLHDBC hdbc, const std::shared_ptr<OdbcHelper>& odbc_helper)
{
    SQLHSTMT stmt = SQL_NULL_HANDLE;
    SQLTCHAR node_id[BUFFER_SIZE] = {0};
    bool is_writer = false;
    SQLREAL cpu_usage = 0;
    SQLINTEGER replica_lag_ms = 0;
    const SQLLEN rt = 0;
    RdsLibResult res;
    std::vector<HostInfo> hosts;
    const DBC* dbc = static_cast<DBC*>(hdbc);

    if (!dbc || !dbc->wrapped_dbc) {
        LOG(ERROR) << "Topology Query passed in null DBC";
        return hosts;
    }

    res = odbc_helper->AllocStmt(dbc->wrapped_dbc, lib_loader_, stmt);

    if (SQL_SUCCEEDED(res.fn_result)) {
        odbc_helper->SQLExecDirect(lib_loader_, stmt, topology_query_);

        odbc_helper->SQLBindCol(lib_loader_, stmt, NODE_ID_COL, SQL_C_TCHAR, &node_id, sizeof(node_id), rt);
        odbc_helper->SQLBindCol(lib_loader_, stmt, IS_WRITER_COL, SQL_BIT, &is_writer, sizeof(is_writer), rt);
        odbc_helper->SQLBindCol(lib_loader_, stmt, CPU_USAGE_COL, SQL_REAL, &cpu_usage, sizeof(cpu_usage), rt);
        odbc_helper->SQLBindCol(lib_loader_, stmt, REPLICA_LAG_COL, SQL_INTEGER, &replica_lag_ms, sizeof(replica_lag_ms), rt);

        res = odbc_helper->SQLFetch(lib_loader_, stmt);
        while (SQL_SUCCEEDED(res.fn_result)) {
            const HostInfo new_host = CreateHost(node_id, is_writer, cpu_usage, replica_lag_ms);
            hosts.push_back(new_host);
            res = odbc_helper->SQLFetch(lib_loader_, stmt);
        }

        odbc_helper->FreeStmt(lib_loader_, stmt);
    }

    LOG_IF(WARNING, hosts.empty()) << "Failed to fetch any instances from topology";
    return hosts;
}

HostInfo ClusterTopologyQueryHelper::CreateHost(
    SQLTCHAR* node_id,
    const bool is_writer,
    const SQLREAL cpu_usage,
    const SQLINTEGER replica_lag_ms)
{
    const uint64_t weight = static_cast<uint64_t>((static_cast<float>(replica_lag_ms) * SCALE_TO_PERCENT) + std::round(cpu_usage));
    const std::string endpoint_url = GetEndpoint(node_id);
    const HostInfo hi = HostInfo(endpoint_url, port, UP, is_writer, weight);
    return hi;
}

std::string ClusterTopologyQueryHelper::GetEndpoint(SQLTCHAR* node_id)
{
    std::string res(endpoint_template_);
    const std::string node_id_str = AS_UTF8_CSTR(node_id);

    if (const size_t pos = res.find(REPLACE_CHAR); pos != std::string::npos) {
        res.replace(pos, 1, node_id_str);
    }
    return res;
}
