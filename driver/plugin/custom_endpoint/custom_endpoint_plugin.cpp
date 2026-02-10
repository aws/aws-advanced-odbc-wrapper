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

#include "custom_endpoint_plugin.h"

#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"

#include <memory>
#include <unordered_map>

// Initialize static members
std::mutex CustomEndpointPlugin::endpoint_monitors_mutex_;
std::unordered_map<std::string, std::pair<unsigned int, std::shared_ptr<CustomEndpointMonitor>>> CustomEndpointPlugin::endpoint_monitors_;

CustomEndpointPlugin::CustomEndpointPlugin(DBC* dbc) : CustomEndpointPlugin(dbc, nullptr) {}

CustomEndpointPlugin::CustomEndpointPlugin(DBC* dbc, BasePlugin* next_plugin) : CustomEndpointPlugin(
    dbc,
    next_plugin,
    nullptr,
    nullptr) {}

CustomEndpointPlugin::CustomEndpointPlugin(
    DBC* dbc,
    BasePlugin* next_plugin,
    const std::shared_ptr<TopologyService>& topology_service,
    const std::shared_ptr<CustomEndpointMonitor>& endpoint_monitor) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "CUSTOM_ENDPOINT";
    this->topology_service_ = topology_service ? topology_service : dbc->topology_service;

    this->cluster_id_ = dbc->conn_attr.at(KEY_CLUSTER_ID);
    this->host_ = dbc->conn_attr.contains(KEY_SERVER) ?
        dbc->conn_attr.at(KEY_SERVER) : "";
    this->region_ = dbc->conn_attr.contains(KEY_CUSTOM_ENDPOINT_REGION) ?
        dbc->conn_attr.at(KEY_CUSTOM_ENDPOINT_REGION) : RdsUtils::GetRdsRegion(this->host_);

    if (this->region_.empty()) {
        throw std::runtime_error("Unable to determine connection region. If you are using a non-standard RDS URL, please set the 'CUSTOM_ENDPOINT_REGION' property");
    }

    this->wait_for_info_ = dbc->conn_attr.contains(KEY_WAIT_FOR_CUSTOM_ENDPOINT_INFO) ?
        dbc->conn_attr.at(KEY_WAIT_FOR_CUSTOM_ENDPOINT_INFO) == VALUE_BOOL_TRUE : false;
    this->wait_duration_ms_ = dbc->conn_attr.contains(KEY_WAIT_FOR_CUSTOM_ENDPOINT_INFO_TIMEOUT_MS) ?
        std::chrono::milliseconds(static_cast<int>(std::strtol(dbc->conn_attr.at(KEY_WAIT_FOR_CUSTOM_ENDPOINT_INFO_TIMEOUT_MS).c_str(), nullptr, 0))) :
        CustomEndpointPlugin::DEFAULT_WAIT_FOR_INFO_TIMEOUT_MS;
    this->refresh_rate_ms_ = dbc->conn_attr.contains(KEY_CUSTOM_ENDPOINT_INTERVAL_MS) ?
        std::chrono::milliseconds(static_cast<int>(std::strtol(dbc->conn_attr.at(KEY_CUSTOM_ENDPOINT_INTERVAL_MS).c_str(), nullptr, 0))) :
        CustomEndpointPlugin::DEFAULT_MONITORING_INTERVAL_MS;
    this->max_refresh_rate_ms_ = dbc->conn_attr.contains(KEY_CUSTOM_ENDPOINT_MAX_INTERVAL_MS) ?
        std::chrono::milliseconds(static_cast<int>(std::strtol(dbc->conn_attr.at(KEY_CUSTOM_ENDPOINT_MAX_INTERVAL_MS).c_str(), nullptr, 0))) :
        CustomEndpointPlugin::DEFAULT_MAX_MONITORING_INTERVAL_MS;
    this->exponential_backoff_rate_ = dbc->conn_attr.contains(KEY_CUSTOM_ENDPOINT_BACKOFF_RATE) ?
        static_cast<int>(std::strtol(dbc->conn_attr.at(KEY_CUSTOM_ENDPOINT_BACKOFF_RATE).c_str(), nullptr, 0)) :
        DEFAULT_EXPONENTIAL_BACKOFF_RATE;

    this->endpoint_monitor_ = endpoint_monitor ? endpoint_monitor : InitEndpointMonitor();
}

CustomEndpointPlugin::~CustomEndpointPlugin()
{
    const std::lock_guard lock_guard(endpoint_monitors_mutex_);
    if (auto itr = endpoint_monitors_.find(this->cluster_id_); itr != endpoint_monitors_.end()) {
        std::pair<unsigned int, std::shared_ptr<CustomEndpointMonitor>>& pair = itr->second;
        if (pair.first == 1) {
            endpoint_monitors_.erase(this->cluster_id_);
            LOG(INFO) << "Shut down Endpoint Monitor for: " <<  this->cluster_id_;
        } else {
            pair.first--;
            LOG(INFO) << "Decremented Endpoint Monitor usage count for: " << this->cluster_id_ << ", to: " << pair.first;
        }
    }
}

SQLRETURN CustomEndpointPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    LOG(INFO) << "Entering Connect";
    const DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    const std::string host = dbc->conn_attr.contains(KEY_SERVER) ?
        dbc->conn_attr.at(KEY_SERVER) : "";

    if (!RdsUtils::IsRdsDns(host)) {
        return next_plugin->Connect(
            ConnectionHandle,
            WindowHandle,
            OutConnectionString,
            BufferLength,
            StringLengthPtr,
            DriverCompletion
        );
    }

    if (this->wait_for_info_) {
        WaitForInfo();
    }

    return next_plugin->Connect(
        ConnectionHandle,
        WindowHandle,
        OutConnectionString,
        BufferLength,
        StringLengthPtr,
        DriverCompletion
    );
}

SQLRETURN CustomEndpointPlugin::Execute(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    LOG(INFO) << "Entering Execute";
    if (this->wait_for_info_) {
        WaitForInfo();
    }
    return next_plugin->Execute(StatementHandle, StatementText, TextLength);
}

std::shared_ptr<CustomEndpointMonitor> CustomEndpointPlugin::InitEndpointMonitor() {
    std::shared_ptr<CustomEndpointMonitor> monitor;
    const std::lock_guard lock_guard(endpoint_monitors_mutex_);
    if (auto itr = endpoint_monitors_.find(this->cluster_id_); itr != endpoint_monitors_.end()) {
        std::pair<unsigned int, std::shared_ptr<CustomEndpointMonitor>>& pair = itr->second;
        pair.first++;
        monitor = pair.second;
        LOG(INFO) << "Incremented Endpoint Monitor usage count for: " << this->cluster_id_ << ", to: " << pair.first;
    } else {
        monitor = std::make_shared<CustomEndpointMonitor>(
            this->topology_service_,
            this->host_,
            this->region_,
            this->refresh_rate_ms_,
            this->max_refresh_rate_ms_,
            this->exponential_backoff_rate_
        );
        const std::pair<unsigned int, std::shared_ptr<CustomEndpointMonitor>> pair = {1, monitor};
        endpoint_monitors_.insert_or_assign(this->cluster_id_, pair);
        LOG(INFO) << "Created Endpoint Monitor for: " << this->cluster_id_;
    }
    return monitor;
}

bool CustomEndpointPlugin::WaitForInfo() {
    bool has_info = endpoint_monitor_->HasInfo();
    if (has_info) {
        return true;
    }

    LOG(INFO) << "Custom endpoint info for " << this->cluster_id_ << " not found. Waiting for " << this->wait_duration_ms_.count() << "ms for monitor to fetch info.";

    const auto wait_for_end = std::chrono::steady_clock::now() + this->wait_duration_ms_;

    while (!has_info && std::chrono::steady_clock::now() < wait_for_end) {
        std::this_thread::sleep_for(WAIT_FOR_INFO_SLEEP_DIR_MS);
        has_info = endpoint_monitor_->HasInfo();
    }

    if (!has_info) {
        LOG(WARNING) << "Custom endpoint timed out after " << this->wait_duration_ms_.count() << "ms while waiting for custom endpoint info for host " << this->cluster_id_;
    }
    return has_info;
}
