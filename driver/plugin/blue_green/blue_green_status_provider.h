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

#ifndef BLUE_GREEN_STATUS_PROVIDER_H_
#define BLUE_GREEN_STATUS_PROVIDER_H_

#include "blue_green_interim_status.h"
#include "blue_green_monitor.h"
#include "blue_green_phase.h"
#include "blue_green_role.h"
#include "blue_green_status.h"
#include "routing/connect/base_connect_routing.h"
#include "routing/execute/base_execute_routing.h"

#include "../../host_info.h"

#include "../../util/concurrent_map.h"
#include "../../util/plugin_service.h"

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

class BlueGreenStatusProvider {
public:
    struct PhaseTimeInfo {
        std::chrono::system_clock::time_point timestamp;
        BlueGreenPhase phase;
    };

    BlueGreenStatusProvider(
        std::shared_ptr<PluginService> plugin_service,
        std::map<std::string, std::string> conn_attr,
        std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> status_cache,
        std::string blue_green_id,
        std::string cluster_id);

    ~BlueGreenStatusProvider();

    void InitMonitoring();

protected:
    std::map<std::string, std::string> GetMonitoringProperties();
    void PrepareStatus(BlueGreenRole role, BlueGreenInterimStatus interim_status);
    void UpdatePhase(BlueGreenRole role, BlueGreenInterimStatus interim_status);
    void UpdateStatusCache();
    void UpdateCorrespondingNodes();
    HostInfo GetWriterHost(BlueGreenRole role);
    std::vector<HostInfo> GetReaderHosts(BlueGreenRole role);
    void UpdateSummaryStatus(BlueGreenRole role, BlueGreenInterimStatus interim_status);
    void UpdateMonitors();
    void UpdateDnsFlags(BlueGreenRole role, BlueGreenInterimStatus interim_status);
    void ResetMonitors(std::atomic<bool>& monitor_reset_completed, std::string event_name);
    int32_t GetContextHash();
    int32_t GetValueHash(int current_hash, std::string value);
    std::string GetHostPort(std::string host, int port);
    std::vector<std::shared_ptr<BaseConnectRouting>> AddSubstituteBlueWithIpAddressConnectRouting();
    BlueGreenStatus GetStatusOfCreated();
    BlueGreenStatus GetStatusOfPreparation();
    BlueGreenStatus GetStatusOfInProgress();
    BlueGreenStatus GetStatusOfPost();
    void CreatePostRouting(std::vector<std::shared_ptr<BaseConnectRouting>>& connect_routing);
    BlueGreenStatus GetStatusOfCompleted();
    void RegisterIamHost(std::string connect_host, std::string iam_host);
    bool IsAlreadySuccessfullyConnected(std::string connect_host, std::string iam_host);
    void StorePhaseTime(BlueGreenPhase phase);
    void StoreBlueDsnUpdateTime();
    void StoreGreenDsnRemoveTime();
    void StoreGreenNodeChangeNameTime();
    void StoreGreenTopologyChangeTime();
    void StoreMonitorResetTime(std::string event_name);
    void LogSwitchoverFinalSummary();
    void ResetContextWhenCompleted();
    void ResetContext();
    void StartSwitchoverTimer();
    bool IsSwitchoverTimerExpired();
    void LogCurrentContext();

private:
    std::mutex process_lock_guard;
    std::shared_ptr<PluginService> plugin_service_;
    std::map<std::string, std::string> conn_attr_;
    std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> status_cache_;
    std::string blue_green_id_;
    std::string cluster_id_;

    std::shared_ptr<ConcurrentMap<std::string, PhaseTimeInfo>> phase_time_map_;

    bool rollback_{false};
    bool blue_dns_update_completed_{false};
    bool green_dns_removed_{false};
    bool green_topology_changed_{false};
    std::atomic<bool> monitor_reset_on_in_progress_completed_{false};
    std::atomic<bool> monitor_reset_on_topology_completed_{false};
    std::atomic<bool> all_green_nodes_changed_{false};

    std::chrono::system_clock::time_point post_status_end_time_ = std::chrono::system_clock::time_point{};
    std::chrono::milliseconds switchover_timeout_ms_;

    std::mutex monitor_mutex_;
    std::shared_ptr<BlueGreenMonitor> monitors_[2];
    std::unordered_map<BlueGreenIntervalRate, std::chrono::milliseconds> check_interval_map_;
    int32_t interim_status_hashes_[2] = {0, 0};
    int32_t last_context_hash_;
    BlueGreenInterimStatus interim_statuses_[2];

    std::atomic<bool> pending_restart_ = false;
    std::shared_ptr<std::thread> reset_monitoring_thread_ = nullptr;

    std::shared_ptr<ConcurrentMap<std::string, std::string>> host_ip_map_;
    std::shared_ptr<ConcurrentMap<std::string, std::pair<HostInfo, HostInfo>>> corresponding_nodes_;
    std::shared_ptr<ConcurrentMap<std::string, BlueGreenRole>> role_by_host_map_;
    std::mutex iam_mutex_;
    std::shared_ptr<ConcurrentMap<std::string, std::set<std::string>>> iam_host_success_connects_map_;
    std::shared_ptr<ConcurrentMap<std::string, std::chrono::system_clock::time_point>> green_node_change_name_times_map_;

    BlueGreenStatus summary_status_;
    BlueGreenPhase latest_status_phase_;
    static std::hash<std::string> hasher;

    static constexpr std::chrono::milliseconds BASELINE_MS = std::chrono::milliseconds(60000);
    static constexpr std::chrono::milliseconds INCREASED_MS = std::chrono::milliseconds(1000);
    static constexpr std::chrono::milliseconds HIGH_MS = std::chrono::milliseconds(100);
    static constexpr std::chrono::milliseconds SWITCHOVER_TIMEOUT_MS = std::chrono::minutes(3);
    static constexpr std::chrono::milliseconds RESET_CHECK_RATE = std::chrono::milliseconds(100);
};

#endif // BLUE_GREEN_STATUS_PROVIDER_H_
