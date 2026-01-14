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

#ifndef CUSTOM_ENDPOINT_PLUGIN_H_
#define CUSTOM_ENDPOINT_PLUGIN_H_

#include "../base_plugin.h"

#include "custom_endpoint_monitor.h"

#include "../../driver.h"
#include "../../util/rds_utils.h"
#include "../../util/topology_service.h"

#include <chrono>
#include <memory>
#include <unordered_map>

class CustomEndpointPlugin : public BasePlugin {
public:
    CustomEndpointPlugin(DBC* dbc);
    CustomEndpointPlugin(DBC* dbc, BasePlugin* next_plugin);
    CustomEndpointPlugin(
        DBC* dbc,
        BasePlugin* next_plugin,
        const std::shared_ptr<TopologyService>& topology_service,
        const std::shared_ptr<CustomEndpointMonitor>& custom_endpoint_monitor);
    ~CustomEndpointPlugin() override;

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

    static inline const std::chrono::milliseconds WAIT_FOR_INFO_SLEEP_DIR_MS = std::chrono::milliseconds(100);
    static inline const std::chrono::milliseconds DEFAULT_MONITORING_INTERVAL_MS = std::chrono::seconds(30);
    static inline const std::chrono::milliseconds DEFAULT_MAX_MONITORING_INTERVAL_MS = std::chrono::minutes(5);
    static inline const int DEFAULT_EXPONENTIAL_BACKOFF_RATE = 2;
    static inline const std::chrono::milliseconds DEFAULT_WAIT_FOR_INFO_TIMEOUT_MS = std::chrono::seconds(5);

private:
    std::shared_ptr<CustomEndpointMonitor> InitEndpointMonitor();
    bool WaitForInfo();

    std::shared_ptr<TopologyService> topology_service_;

    std::string cluster_id_;
    std::string region_;
    std::string host_;

    std::shared_ptr<CustomEndpointMonitor> endpoint_monitor_;
    bool wait_for_info_;
    std::chrono::milliseconds wait_duration_ms_;
    std::chrono::milliseconds refresh_rate_ms_;
    std::chrono::milliseconds max_refresh_rate_ms_;
    int exponential_backoff_rate_;

    static std::mutex endpoint_monitors_mutex_;
    static std::unordered_map<std::string, std::pair<unsigned int, std::shared_ptr<CustomEndpointMonitor>>> endpoint_monitors_;
};

#endif // CUSTOM_ENDPOINT_PLUGIN_H_
