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

#include "blue_green_plugin.h"
#include "blue_green_status_provider.h"

#include "../../driver.h"

#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/map_utils.h"
#include "../../util/plugin_service.h"

std::mutex BlueGreenPlugin::provider_lock_;
std::map<std::string, std::pair<unsigned int, std::shared_ptr<BlueGreenStatusProvider>>> BlueGreenPlugin::status_providers_map_;
std::shared_ptr<SlidingCacheMap<std::string, BlueGreenStatus>> BlueGreenPlugin::status_map_ = std::make_shared<SlidingCacheMap<std::string, BlueGreenStatus>>();

BlueGreenPlugin::BlueGreenPlugin(DBC* dbc) : BlueGreenPlugin(dbc, nullptr) {}

BlueGreenPlugin::BlueGreenPlugin(DBC* dbc, BasePlugin* next_plugin) : BasePlugin(dbc, next_plugin) {
    this->plugin_name = "BLUE_GREEN";
    this->plugin_service_ = dbc->plugin_service;
    this->cluster_id_ = plugin_service_->GetClusterId();
    this->conn_attr_ = dbc->conn_attr;
    this->blue_green_id_ = MapUtils::GetStringValue(this->conn_attr_, KEY_BG_ID, "BG-1");
}

BlueGreenPlugin::~BlueGreenPlugin() {
    std::lock_guard<std::mutex> lock_guard(provider_lock_);
    if (auto itr = status_providers_map_.find(this->blue_green_id_); itr != status_providers_map_.end()) {
        std::pair<unsigned int, std::shared_ptr<BlueGreenStatusProvider>>& pair = itr->second;
        if (pair.first == 1) {
            status_providers_map_.erase(this->blue_green_id_);
            LOG(INFO) << "Shut down Blue Green Status Provider: " << this->blue_green_id_;
        } else {
            pair.first--;
            LOG(INFO) << "Decremented Blue Green Status Provider count for: " << this->blue_green_id_ << ", to: " << pair.first;
        }
    }
}

SQLRETURN BlueGreenPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    LOG(INFO) << "Entering Connect";
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    if (dbc->conn_attr.contains(KEY_INTERNAL_BG_FORCE_CONNECT)) {
        return next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    this->ResetRoutingTiming();

    this->blue_green_status_ = status_map_->Get(this->blue_green_id_);
    if (this->blue_green_status_.GetCurrentPhase().GetPhase() == BlueGreenPhase::UNKNOWN) {
        return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }
    std::string conn_host = dbc->conn_attr.at(KEY_SERVER);
    BlueGreenRole host_role = this->blue_green_status_.GetRole(conn_host);
    if (host_role.GetRole() == BlueGreenRole::UNKNOWN) {
        return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    std::vector<std::shared_ptr<BaseConnectRouting>> connect_routes = this->blue_green_status_.GetConnectRoutes();
    if (connect_routes.empty()) {
        return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    SQLRETURN rc = SQL_ERROR;
    this->start_time_ = std::chrono::system_clock::now();
    for (auto& route : connect_routes) {
        rc = route->Connect(dbc, HostInfo(), this->odbc_helper_, this->status_map_);
        if (SQL_SUCCEEDED(rc)) {
            break;
        }
        this->blue_green_status_ = status_map_->Get(this->blue_green_id_);
        if (this->blue_green_status_.GetCurrentPhase().GetPhase() == BlueGreenPhase::UNKNOWN) {
            this->end_time_ = std::chrono::system_clock::now();
            return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
        }
    }
    this->end_time_ = std::chrono::system_clock::now();

    if (!SQL_SUCCEEDED(rc)) {
        return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }
    return rc;
}

SQLRETURN BlueGreenPlugin::Execute(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    LOG(INFO) << "Entering Execute";
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    this->ResetRoutingTiming();
    this->InitProvider();

    this->blue_green_status_ = status_map_->Get(this->blue_green_id_);
    if (this->blue_green_status_.GetCurrentPhase().GetPhase() == BlueGreenPhase::UNKNOWN) {
        return next_plugin->Execute(StatementHandle, StatementText, TextLength);
    }

    std::vector<std::shared_ptr<BaseExecuteRouting>> execute_routes = this->blue_green_status_.GetExecuteRoutes();
    if (execute_routes.empty()) {
        return next_plugin->Execute(StatementHandle, StatementText, TextLength);
    }

    SQLRETURN rc = SQL_ERROR;
    for (auto& route : execute_routes) {
        rc = route->Execute(stmt, this->odbc_helper_, this->status_map_);
        if (SQL_SUCCEEDED(rc)) {
            break;
        }
        this->blue_green_status_ = status_map_->Get(this->blue_green_id_);
        if (this->blue_green_status_.GetCurrentPhase().GetPhase() == BlueGreenPhase::UNKNOWN) {
            this->end_time_ = std::chrono::system_clock::now();
            return next_plugin->Execute(StatementHandle, StatementText, TextLength);
        }
    }
    this->end_time_ = std::chrono::system_clock::now();
    return rc;
}

int64_t BlueGreenPlugin::GetHoldTime() {
    if (this->start_time_ == std::chrono::system_clock::time_point{}) {
        return 0;
    }
    return this->end_time_ == std::chrono::system_clock::time_point{} ?
        (std::chrono::system_clock::now() - this->start_time_).count()
        : (this->end_time_ - this->start_time_).count();
}

void BlueGreenPlugin::ResetRoutingTiming() {
    this->start_time_ = std::chrono::system_clock::time_point{};
    this->end_time_ = std::chrono::system_clock::time_point{};
}

SQLRETURN BlueGreenPlugin::InitConnection(
    SQLHDBC ConnectionHandle,
    SQLHWND WindowHandle,
    SQLTCHAR* OutConnectionString,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* StringLengthPtr,
    SQLUSMALLINT DriverCompletion)
{
    SQLRETURN rc = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

    if (SQL_SUCCEEDED(rc)) {
        this->InitProvider();
    }

    return rc;
}

void BlueGreenPlugin::InitProvider() {
    if (!this->status_provider_) {
        this->status_provider_ = GetOrCreateProvider();
    }
    this->status_provider_->InitMonitoring();
}

std::shared_ptr<BlueGreenStatusProvider> BlueGreenPlugin::GetOrCreateProvider() {
    std::lock_guard<std::mutex> lock_guard(provider_lock_);
    std::shared_ptr<BlueGreenStatusProvider> provider;
    if (auto itr = status_providers_map_.find(this->blue_green_id_); itr != status_providers_map_.end()) {
        std::pair<unsigned int, std::shared_ptr<BlueGreenStatusProvider>>& pair = itr->second;
        pair.first++;
        provider = pair.second;
        LOG(INFO) << "Incremented Blue Green Status Provider count for: " << this->blue_green_id_ << ", to: " << pair.first;
    } else {
        provider = std::make_shared<BlueGreenStatusProvider>(
            this->plugin_service_,
            this->conn_attr_,
            this->status_map_,
            this->blue_green_id_,
            this->cluster_id_);
        std::pair pair = {1, provider};
        status_providers_map_.insert_or_assign(this->blue_green_id_, pair);
        LOG(INFO) << "Created Blue Green Status Provider: " << this->blue_green_id_;
    }
    return provider;
}
