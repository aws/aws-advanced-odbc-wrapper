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

#ifndef FAILOVER_PLUGIN_H_
#define FAILOVER_PLUGIN_H_

#include "../base_plugin.h"
#include "cluster_topology_monitor.h"
#include "../../driver.h"
#include "../../dialect/dialect.h"
#include "../../host_selector/host_selector.h"
#include "../../host_info.h"
#include "../../util/connection_string_keys.h"
#include "../../util/sliding_cache_map.h"

typedef enum {
    STRICT_READER,
    STRICT_WRITER,
    READER_OR_WRITER,
    UNKNOWN_FAILOVER_MODE
} FailoverMode;

static std::map<RDS_STR, FailoverMode> const failover_mode_table = {
    {VALUE_FAILOVER_MODE_STRICT_READER,     FailoverMode::STRICT_READER},
    {VALUE_FAILOVER_MODE_STRICT_WRITER,     FailoverMode::STRICT_WRITER},
    {VALUE_FAILOVER_MODE_READER_OR_WRITER,  FailoverMode::READER_OR_WRITER}
};

class FailoverPlugin : public BasePlugin {
public:
    FailoverPlugin() = default;
    explicit FailoverPlugin(DBC* dbc);
    FailoverPlugin(DBC* dbc, BasePlugin* next_plugin);
    FailoverPlugin(
        DBC* dbc,
        BasePlugin* next_plugin,
        FailoverMode failover_mode, const std::shared_ptr<Dialect>& dialect,
        const std::shared_ptr<HostSelector>& host_selector,
        const std::shared_ptr<ClusterTopologyQueryHelper>& topology_query_helper,
        const std::shared_ptr<ClusterTopologyMonitor>& topology_monitor
    );
    ~FailoverPlugin() override;

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

    SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText,
        SQLINTEGER     TextLength) override;

    static unsigned int GetTopologyMonitorCount(const std::string& cluster_id);
private:
    static constexpr int MAX_STATE_LENGTH = 32;
    static constexpr int MAX_MSG_LENGTH = 1024;
    static inline const std::chrono::milliseconds
        DEFAULT_FAILOVER_TIMEOUT_MS = std::chrono::seconds(30);

    bool CheckShouldFailover(const RDS_CHAR* sql_state);
    static void RemoveHostCandidate(const std::string& host, std::vector<HostInfo>& candidates);
    bool FailoverReader(DBC* hdbc);
    bool FailoverWriter(DBC* hdbc);
    static bool ConnectToHost(DBC* hdbc, const std::string& host_string);

    static std::string InitClusterId(std::map<RDS_STR, RDS_STR>& conn_info);
    static FailoverMode InitFailoverMode(std::map<RDS_STR, RDS_STR>& conn_info);
    std::shared_ptr<HostSelector> InitHostSelectorStrategy(std::map<RDS_STR, RDS_STR>& conn_info);
    std::shared_ptr<ClusterTopologyQueryHelper> InitQueryHelper(DBC* dbc);
    std::shared_ptr<ClusterTopologyMonitor> InitTopologyMonitor(DBC* dbc);

    HostInfo curr_host_;
    std::chrono::milliseconds failover_timeout_ms_;
    std::string cluster_id_;
    std::shared_ptr<Dialect> dialect_;
    HostSelectorStrategies host_selector_strategy_;
    std::shared_ptr<HostSelector> host_selector_;
    std::shared_ptr<ClusterTopologyQueryHelper> topology_query_helper_;
    std::shared_ptr<ClusterTopologyMonitor> topology_monitor_;
    FailoverMode failover_mode_ = UNKNOWN_FAILOVER_MODE;

    static inline std::shared_ptr<SlidingCacheMap<std::string, std::vector<HostInfo>>> topology_map_
        = std::make_shared<SlidingCacheMap<std::string, std::vector<HostInfo>>>();

    static std::mutex topology_monitors_mutex_;
    static SlidingCacheMap<std::string, std::pair<unsigned int, std::shared_ptr<ClusterTopologyMonitor>>> topology_monitors_;
};

#endif // FAILOVER_PLUGIN_H_
