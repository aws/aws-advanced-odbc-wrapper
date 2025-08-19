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

#ifndef CLUSTER_TOPOLOGY_MONITOR_H
#define CLUSTER_TOPOLOGY_MONITOR_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef WIN32
    #include <windows.h>
#endif
#include <sql.h>
#include <sqltypes.h>

#include "cluster_topology_query_helper.h"

#include "../base_plugin.h"
#include "../../driver.h"
#include "../../host_info.h"
#include "../../util/logger_wrapper.h"
#include "../../util/sliding_cache_map.h"
#include "../../util/rds_strings.h"

struct DBC;
struct ENV;

class ClusterTopologyMonitor {
public:
    ClusterTopologyMonitor(DBC* dbc,
        const std::shared_ptr<SlidingCacheMap<std::string, std::vector<HostInfo>>>& topology_map,
        const std::shared_ptr<ClusterTopologyQueryHelper>& query_helper);
    ~ClusterTopologyMonitor();

    virtual void SetClusterId(const std::string& cluster_id);
    virtual std::vector<HostInfo> ForceRefresh(bool verify_writer, uint32_t timeout_ms);
    virtual std::vector<HostInfo> ForceRefresh(SQLHDBC hdbc, uint32_t timeout_ms);

    virtual void StartMonitor();

protected:
    void Run();
    std::vector<HostInfo> WaitForTopologyUpdate(uint32_t timeout_ms);
    void DelayMainThread(bool use_high_refresh_rate);
    std::vector<HostInfo> FetchTopologyUpdateCache(SQLHDBC hdbc);
    void UpdateTopologyCache(const std::vector<HostInfo>& hosts);
    RDS_STR ConnForHost(const std::string& new_host);

private:
    class NodeMonitoringThread;
    std::shared_ptr<ClusterTopologyQueryHelper> query_helper_;
    bool InPanicMode();
    std::vector<HostInfo> OpenAnyConnGetHosts();
    void CleanUpDbc(std::shared_ptr<SQLHDBC>& dbc);

    bool HandlePanicMode();
    bool HandleRegularMode();
    void HandleIgnoreTopologyTiming();
    void InitNodeMonitors();
    bool GetPossibleWriterConn();

    std::shared_ptr<RdsLibLoader> lib_loader_;
    BasePlugin* plugin_head_;
    // Topology Tracking
    std::string cluster_id_;
    std::map<RDS_STR, RDS_STR> connection_attributes_;

    // SlidingCacheMap internally is thread safe
    std::shared_ptr<SlidingCacheMap<std::string, std::vector<HostInfo>>> topology_map_;

    // Track Update Request
    std::atomic<bool> request_update_topology_;
    std::mutex request_update_topology_mutex_;
    std::condition_variable request_update_topology_cv_;
    const std::chrono::milliseconds TOPOLOGY_REQUEST_WAIT_MS = std::chrono::milliseconds(50);

    // Track Topology Updated
    std::mutex topology_updated_mutex_;
    std::condition_variable topology_updated_;
    const std::chrono::milliseconds TOPOLOGY_UPDATE_WAIT_MS = std::chrono::milliseconds(1000);

    std::atomic<std::chrono::steady_clock::time_point> ignore_topology_request_end_ms_;
    std::chrono::milliseconds ignore_topology_request_ms_ = std::chrono::seconds(30);
    std::chrono::steady_clock::time_point high_refresh_end_time_;
    std::chrono::milliseconds high_refresh_rate_ms_ = std::chrono::milliseconds(100);
    const std::chrono::seconds high_refresh_rate_after_panic_ = std::chrono::seconds(30);
    std::chrono::milliseconds refresh_rate_ms_ = std::chrono::seconds(30);

    // Main Thread
    std::shared_ptr<std::thread> monitoring_thread_;
    std::atomic<bool> is_running_;
    // Children / Node Threads
    std::map<std::string, std::shared_ptr<NodeMonitoringThread>> node_monitoring_threads_;
    std::atomic<bool> node_threads_stop_;

    // Children Thread Connections & Host Info
    std::shared_ptr<SQLHDBC> node_threads_writer_hdbc_;
    std::shared_ptr<HostInfo> node_threads_writer_host_info_;
    std::shared_ptr<SQLHDBC> node_threads_reader_hdbc_;
    std::shared_ptr<std::vector<HostInfo>> node_threads_latest_topology_;

    std::mutex node_threads_writer_hdbc_mutex_;
    std::mutex node_threads_writer_host_info_mutex_;
    std::mutex node_threads_reader_hdbc_mutex_;
    std::mutex node_threads_latest_topology_mutex_;

    // TODO(yuenhcol), review if these can be done without mutex/atomics
    // There should be only at most 1 thread interacting with these
    // Connection Information for main thread
    std::atomic<bool> is_writer_connection_;
    SQLHENV henv_;
    SQLHDBC hdbc_;
    std::mutex hdbc_mutex_;
    std::shared_ptr<SQLHDBC> main_hdbc_;
    std::shared_ptr<HostInfo> main_writer_host_info_;
};

class ClusterTopologyMonitor::NodeMonitoringThread {
public:
    NodeMonitoringThread(ClusterTopologyMonitor* monitor, const std::shared_ptr<HostInfo>& host_info,
        const std::shared_ptr<HostInfo>& writer_host_info);
    ~NodeMonitoringThread();

private:
    void Run();
    void HandleReconnect();
    void HandleWriterConn();
    void HandleReaderConn();
    void ReaderThreadFetchTopology();

    ClusterTopologyMonitor* main_monitor_;
    std::shared_ptr<HostInfo> host_info_;
    std::shared_ptr<HostInfo> writer_host_info_;
    bool writer_changed_ = false;
    std::shared_ptr<std::thread> node_thread_;
    SQLHDBC hdbc_ = SQL_NULL_HDBC;
    bool reader_update_topology_ = false;

    const uint32_t THREAD_SLEEP_MS_ = 100;
};

#endif // CLUSTER_TOPOLOGY_MONITOR_H
