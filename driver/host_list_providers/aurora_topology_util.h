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

#ifndef AURORA_TOPOLOGY_UTILS_H_
#define AURORA_TOPOLOGY_UTILS_H_

#include "topology_util.h"

#include <vector>
#include <memory>

#include "../dialect/dialect.h"

#include "../host_info.h"
#include "../util/odbc_helper.h"

class AuroraTopologyUtil : public TopologyUtil {
public:
    AuroraTopologyUtil(const std::shared_ptr<OdbcHelper> &odbc_helper, const std::shared_ptr<Dialect> &dialect);
    virtual std::vector<HostInfo> GetHosts(SQLHSTMT stmt, const HostInfo& initial_host, const HostInfo& host_template) override;
    virtual HostInfo CreateHost(SQLTCHAR* node_id, bool is_writer, SQLREAL cpu_usage, SQLINTEGER replica_lag_ms, const HostInfo& initial_host, const HostInfo& host_template);

private:
    static constexpr char REPLACE_CHAR = '?';
    static constexpr float SCALE_TO_PERCENT = 100.0;

    static constexpr int NODE_ID_COL = 1;
    static constexpr int IS_WRITER_COL = 2;
    static constexpr int CPU_USAGE_COL = 3;
    static constexpr int REPLICA_LAG_COL = 4;
};

#endif // AURORA_TOPOLOGY_UTILS_H_
