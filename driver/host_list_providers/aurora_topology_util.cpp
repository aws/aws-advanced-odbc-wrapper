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

#include "aurora_topology_util.h"

#include "../dialect/dialect.h"
#include "../host_info.h"
#include "../util/odbc_helper.h"

#include <cmath>

AuroraTopologyUtil::AuroraTopologyUtil(const std::shared_ptr<OdbcHelper>& odbc_helper, const std::shared_ptr<Dialect>& dialect) : TopologyUtil(odbc_helper, dialect) {}

std::vector<HostInfo> AuroraTopologyUtil::GetHosts(SQLHSTMT stmt, const HostInfo& initial_host, const HostInfo& host_template) {
    SQLTCHAR node_id[BUFFER_SIZE] = {0};
    bool is_writer = false;
    SQLREAL cpu_usage = 0;
    SQLLEN len = 0;
    SQLINTEGER replica_lag_ms = 0;

    this->odbc_helper_->BindCol(&stmt, NODE_ID_COL, SQL_C_TCHAR, &node_id, sizeof(node_id), &len);
    this->odbc_helper_->BindCol(&stmt, IS_WRITER_COL, SQL_BIT, &is_writer, sizeof(is_writer), &len);
    this->odbc_helper_->BindCol(&stmt, CPU_USAGE_COL, SQL_REAL, &cpu_usage, sizeof(cpu_usage), &len);
    this->odbc_helper_->BindCol(&stmt, REPLICA_LAG_COL, SQL_INTEGER, &replica_lag_ms, sizeof(replica_lag_ms), &len);

    std::vector<HostInfo> hosts;
    RdsLibResult res = this->odbc_helper_->Fetch(&stmt);
    while (SQL_SUCCEEDED(res.fn_result)) {
        const HostInfo new_host = CreateHost(node_id, is_writer, cpu_usage, replica_lag_ms, initial_host, host_template);
        hosts.push_back(new_host);
        res = this->odbc_helper_->Fetch(&stmt);
    }
    return hosts;
}

HostInfo AuroraTopologyUtil::CreateHost(
    SQLTCHAR* node_id,
    const bool is_writer,
    const SQLREAL cpu_usage,
    const SQLINTEGER replica_lag_ms,
    const HostInfo& initial_host,
    const HostInfo& host_template)
{
    const std::string node_id_str = AS_UTF8_CSTR(node_id);
    std::string endpoint_url = host_template.GetHost();
    if (const size_t pos = endpoint_url.find(REPLACE_CHAR); pos != std::string::npos) {
        endpoint_url.replace(pos, 1, node_id_str);
    }
    const int port = host_template.GetPort() != HostInfo::NO_PORT ?
        host_template.GetPort() : initial_host.GetPort();
    const uint64_t weight = static_cast<uint64_t>((static_cast<float>(replica_lag_ms) * SCALE_TO_PERCENT) + std::round(cpu_usage));

    return TopologyUtil::CreateHost(endpoint_url, port, UP, is_writer ? WRITER : READER, weight, std::chrono::steady_clock::now());
}
