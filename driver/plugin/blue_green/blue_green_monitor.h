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

#ifndef BLUE_GREEN_MONITOR_H_
#define BLUE_GREEN_MONITOR_H_

#include "blue_green_interim_status.h"
#include "blue_green_phase.h"
#include "blue_green_role.h"

#include "../../driver.h"
#include "../../host_info.h"

#include "../../dialect/dialect.h"

#include "../../host_list_providers/host_list_provider.h"

#include "../../plugin/base_plugin.h"

#include "../../util/concurrent_map.h"
#include "../../util/odbc_helper.h"
#include "../../util/plugin_service.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

typedef enum {
    BASELINE,
    INCREASED,
    HIGH
} BlueGreenIntervalRate;

class BlueGreenMonitor {
public:
    BlueGreenMonitor(
        const std::shared_ptr<PluginService>& plugin_service,
        BlueGreenRole role,
        std::string blue_green_id,
        HostInfo initial_host_info,
        std::map<std::string, std::string> conn_attr,
        std::unordered_map<BlueGreenIntervalRate, std::chrono::milliseconds> check_interval_map,
        std::function<void(BlueGreenRole, BlueGreenInterimStatus)> on_status_change_function);
    ~BlueGreenMonitor();

    void StartMonitoring();
    void SetIntervalRate(BlueGreenIntervalRate interval_rate);
    void SetCollectIp(bool collect_ip);
    void SetCollectTopology(bool collect_topology);
    void SetUseIp(bool use_ip);
    void ResetCollectedData();
    void Stop();
    bool IsStop();

protected:
    void Run();
    void Delay(std::chrono::milliseconds delay_ms);
    void CollectHostIp();
    void UpdateIpAddressFlags();
    std::string GetIpAddress(std::string host);
    void CollectTopology();
    void CollectStatus();
    void OpenConnection();
    void NotifyChanges();
    void InitHostListProvider();

private:
    struct StatusInfo {
        std::string version;
        std::string endpoint;
        int port;
        BlueGreenPhase phase;
        BlueGreenRole role;
    };

    std::shared_ptr<PluginService> plugin_service_;
    std::shared_ptr<BasePlugin> plugin_head_;
    std::shared_ptr<OdbcHelper> odbc_helper_;
    std::shared_ptr<DialectBlueGreen> dialect_blue_green_;
    std::string blue_green_id_;
    HostInfo initial_host_info_;
    std::map<std::string, std::string> conn_attr_;

    BlueGreenRole current_role_;
    BlueGreenPhase current_phase_ = BlueGreenPhase::NOT_CREATED;
    std::string current_version_ = "1.0";
    int current_port_ = -1;

    std::unordered_map<BlueGreenIntervalRate, std::chrono::milliseconds> check_interval_map_;
    std::atomic<BlueGreenIntervalRate> interval_rate_ = BlueGreenIntervalRate::BASELINE;
    std::atomic<bool> collect_ip_ = true;
    std::atomic<bool> collect_topology_ = true;
    std::atomic<bool> connecting_host_info_correct_ = false;
    std::atomic<bool> use_ip_ = false;
    std::atomic<bool> in_panic_mode_ = false;

    bool all_start_topology_ip_changed_ = false;
    bool all_start_topology_endpoints_removed_ = false;
    bool all_topology_changed_ = false;

    std::function<void(BlueGreenRole, BlueGreenInterimStatus)> on_status_change_function_;

    std::vector<HostInfo> initial_topology_;
    std::vector<HostInfo> current_topology_;
    std::map<std::string, std::string> initial_ip_host_map_;
    std::map<std::string, std::string> current_ip_host_map_;
    std::set<std::string> host_names_;
    // Shared Resources
    std::mutex initial_ip_host_map_mutex_;
    std::mutex initial_topology_mutex_;
    std::mutex host_names_mutex_;

    HostInfo connecting_host_info_;
    std::string connecting_ip_;

    std::mutex monitor_mutex_;

    std::mutex finish_mutex_;
    std::condition_variable finish_cv_;
    std::atomic<bool> class_running_ = false;
    std::atomic<bool> thread_running_ = false;
    std::condition_variable sleep_cv_;
    std::mutex sleep_mutex_;
    std::shared_ptr<HostListProvider> host_list_provider_;
    std::shared_ptr<std::thread> monitoring_thread_;
    SQLHENV henv_ = SQL_NULL_HENV;
    std::mutex hdbc_mutex_;
    SQLHDBC hdbc_ = SQL_NULL_HDBC;

    void ClearMemory(void* dest, size_t count);

    static constexpr int BUFFER_SIZE = 1024;
    static inline const std::string BG_CLUSTER_ID =
        "ae86f030-a260-40e5-a5cb-92c95d55f333";
    static constexpr std::chrono::milliseconds MINIMUM_DELAY_MS =
        std::chrono::milliseconds(50);
    static inline const std::chrono::milliseconds DEFAULT_INTERVAL_MS =
        std::chrono::minutes(5);
};

#endif // BLUE_GREEN_MONITOR_H_
