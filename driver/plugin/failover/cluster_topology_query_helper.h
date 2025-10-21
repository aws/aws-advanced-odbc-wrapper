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

#ifndef CLUSTER_TOPOLOGY_QUERY_HELPER_H_
#define CLUSTER_TOPOLOGY_QUERY_HELPER_H_

#include <vector>

#ifdef WIN32
    #include <windows.h>
#endif
#include <sqltypes.h>

#include "../../util/rds_lib_loader.h"
#include "../../util/rds_strings.h"
#include "../../driver.h"
#include "../../host_info.h"

class ClusterTopologyQueryHelper {
public:
    ClusterTopologyQueryHelper(const std::shared_ptr<RdsLibLoader> &lib_loader, int port, std::string endpoint_template, RDS_STR topology_query, RDS_STR writer_id_query, RDS_STR node_id_query);
    virtual std::string GetWriterId(SQLHDBC hdbc);
    virtual std::vector<HostInfo> QueryTopology(SQLHDBC hdbc);
    virtual HostInfo CreateHost(SQLTCHAR* node_id, bool is_writer, SQLREAL cpu_usage, SQLREAL replica_lag_ms);
    virtual std::string GetEndpoint(SQLTCHAR* node_id);

private:
    std::shared_ptr<RdsLibLoader> lib_loader_;
    const int port;

    // Query & Template to be passed in from caller, below are examples for APG
    // ?.cluster-<Cluster-ID>.<Region>.rds.amazonaws.com
    std::string endpoint_template_;
    // SELECT SERVER_ID, CASE WHEN SESSION_ID = 'MASTER_SESSION_ID' THEN TRUE ELSE FALSE END, CPU, COALESCE(REPLICA_LAG_IN_MSEC, 0) FROM aurora_replica_status() WHERE EXTRACT(EPOCH FROM(NOW() - LAST_UPDATE_TIMESTAMP)) <= 300 OR SESSION_ID = 'MASTER_SESSION_ID' OR LAST_UPDATE_TIMESTAMP IS NULL
    RDS_STR topology_query_;
    // SELECT SERVER_ID FROM aurora_replica_status() WHERE SESSION_ID = 'MASTER_SESSION_ID' AND SERVER_ID = aurora_db_instance_identifier()
    RDS_STR writer_id_query_;
    // SELECT aurora_db_instance_identifier()
    RDS_STR node_id_query_;

    static constexpr char REPLACE_CHAR = '?';

    static constexpr int BUFFER_SIZE = 1024;
    static constexpr uint64_t SCALE_TO_PERCENT = 100L;

    // Topology Query
    static constexpr int NODE_ID_COL = 1;
    static constexpr int IS_WRITER_COL = 2;
    static constexpr int CPU_USAGE_COL = 3;
    static constexpr int REPLICA_LAG_COL = 4;
};

#endif // CLUSTER_TOPOLOGY_QUERY_HELPER_H_
