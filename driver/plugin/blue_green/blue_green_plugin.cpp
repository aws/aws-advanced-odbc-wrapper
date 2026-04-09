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

#include "../../util/auth_provider.h"

#include "blue_green_plugin.h"
#include "blue_green_status_provider.h"

#include "../../driver.h"

#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/map_utils.h"
#include "../../util/plugin_chain_builder.h"
#include "../../util/plugin_service.h"

std::mutex BlueGreenPlugin::provider_lock_;
std::map<std::string, std::pair<unsigned int, std::shared_ptr<BlueGreenStatusProvider>>> BlueGreenPlugin::status_providers_map_;
std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> BlueGreenPlugin::status_map_ = std::make_shared<ConcurrentMap<std::string, BlueGreenStatus>>();

BlueGreenPlugin::BlueGreenPlugin(DBC* dbc) : BlueGreenPlugin(dbc, nullptr) {}

BlueGreenPlugin::BlueGreenPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin) : BasePlugin(dbc, next_plugin) {
    this->plugin_name = "BLUE_GREEN";
    this->conn_attr_ = dbc->conn_attr;
    this->plugin_service_ = dbc->plugin_service;
    this->odbc_helper_ = dbc->plugin_service->GetOdbcHelper();
    this->blue_green_id_ = MapUtils::GetStringValue(this->conn_attr_, KEY_BG_ID, "BG-1");
    this->cluster_id_ = dbc->plugin_service->GetClusterId();
}

BlueGreenPlugin::~BlueGreenPlugin() {
    std::lock_guard<std::mutex> lock_guard(provider_lock_);
    if (auto itr = status_providers_map_.find(this->blue_green_id_); itr != status_providers_map_.end()) {
        std::pair<unsigned int, std::shared_ptr<BlueGreenStatusProvider>>& pair = itr->second;
        if (pair.first == 1) {
            status_providers_map_.erase(this->blue_green_id_);
            status_map_->Erase(this->blue_green_id_);
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
    this->InitProvider();
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    if (dbc->conn_attr.contains(KEY_MONITORING_CONN_UUID)) {
        return next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    this->ResetRoutingTiming();

    this->blue_green_status_ = status_map_->Get(this->blue_green_id_);
    if (this->blue_green_status_.GetCurrentPhase().GetPhase() == BlueGreenPhase::UNKNOWN) {
        LOG(INFO) << "Default connection, no status found: " << this->blue_green_id_;
        return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }
    std::string conn_host = dbc->conn_attr.at(KEY_SERVER);
    BlueGreenRole host_role = this->blue_green_status_.GetRole(conn_host);
    if (host_role.GetRole() == BlueGreenRole::UNKNOWN) {
        LOG(INFO) << "Default connection, unexpected role: UNKNOWN, host: " << conn_host;
        return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    std::vector<std::shared_ptr<BaseConnectRouting>> connect_routes = this->blue_green_status_.GetConnectRoutes();
    if (connect_routes.empty()) {
        LOG(INFO) << "Default connection, no routes found for: " << conn_host;
        return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    auto route_itr = std::find_if(connect_routes.begin(), connect_routes.end(),
        [&conn_host, &host_role](const std::shared_ptr<BaseConnectRouting>& route) {
            return route->IsMatch(conn_host, host_role);
        });

    if (route_itr == connect_routes.end()) {
        LOG(INFO) << "Default connection, no routes matched for role: " << host_role.ToString() << ", host: " << conn_host;
        return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    const std::map<std::string, std::string> original_conn_attr = dbc->conn_attr;

    SQLRETURN rc = SQL_ERROR;
    this->start_time_ = std::chrono::steady_clock::now();
    try {
        std::string last_failed_route;
        while (route_itr != connect_routes.end() && !SQL_SUCCEEDED(rc)) {
            std::string current_route = (*route_itr)->ToString();
            rc = (*route_itr)->Connect(dbc, HostInfo(conn_host), this->odbc_helper_, this->status_map_);
            LOG(INFO) << "Connection route returned: " << std::to_string(rc);
            if (!SQL_SUCCEEDED(rc)) {
                this->blue_green_status_ = status_map_->Get(this->blue_green_id_);
                if (this->blue_green_status_.GetCurrentPhase().GetPhase() == BlueGreenPhase::UNKNOWN) {
                    this->end_time_ = std::chrono::steady_clock::now();
                    LOG(WARNING) << "Default connection, statuses reset, routes cleared for role: " << host_role.ToString() << ", host: " << conn_host;
                    dbc->conn_attr = original_conn_attr;
                    return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
                }

                connect_routes = this->blue_green_status_.GetConnectRoutes();
                route_itr = std::find_if(connect_routes.begin(), connect_routes.end(),
                    [&conn_host, &host_role](const std::shared_ptr<BaseConnectRouting>& route) {
                        return route->IsMatch(conn_host, host_role);
                    });

                if (route_itr != connect_routes.end() && (*route_itr)->ToString() == last_failed_route) {
                    route_itr = connect_routes.end();
                }
                last_failed_route = current_route;
            }
        }
        if (!SQL_SUCCEEDED(rc)) {
            LOG(WARNING) << "Default connection, alternative routes unsuccessful for role: " << host_role.ToString() << ", host: " << conn_host;
            dbc->conn_attr = original_conn_attr;
            return InitConnection(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
        }
        // Restore conn_attr even on success — the routing modified SERVER/IAM_HOST/PORT
        dbc->conn_attr = original_conn_attr;
    } catch (const std::exception& ex) {
        dbc->conn_attr = original_conn_attr;
        CLEAR_DBC_ERROR(dbc);
        std::string error_message("Blue/Green Connect route failed: ");
        error_message += ex.what();
        dbc->err = new ERR_INFO(error_message.c_str(), ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        LOG(ERROR) << error_message;
        rc = SQL_ERROR;
    }
    this->end_time_ = std::chrono::steady_clock::now();
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
        LOG(INFO) << "Default execution, no status found: " << this->blue_green_id_;
        return next_plugin->Execute(StatementHandle, StatementText, TextLength);
    }

    std::string conn_host = stmt->dbc->conn_attr.at(KEY_SERVER);
    BlueGreenRole host_role = this->blue_green_status_.GetRole(conn_host);
    if (host_role.GetRole() == BlueGreenRole::UNKNOWN) {
        LOG(INFO) << "Default execution, unexpected role: UNKNOWN, host: " << conn_host;
        return next_plugin->Execute(StatementHandle, StatementText, TextLength);
    }

    std::vector<std::shared_ptr<BaseExecuteRouting>> execute_routes = this->blue_green_status_.GetExecuteRoutes();
    if (execute_routes.empty()) {
        LOG(INFO) << "Default execution, no routes found for: " << conn_host;
        return next_plugin->Execute(StatementHandle, StatementText, TextLength);
    }

    auto route_itr = std::find_if(execute_routes.begin(), execute_routes.end(),
        [&conn_host, &host_role](const std::shared_ptr<BaseExecuteRouting> route) {
            return route->IsMatch(conn_host, host_role);
        });

    if (route_itr == execute_routes.end()) {
        LOG(INFO) << "Default execution, no routes matched for role: " << host_role.ToString() << ", host: " << conn_host;
        return next_plugin->Execute(StatementHandle, StatementText, TextLength);
    }

    SQLRETURN rc = SQL_ERROR;
    this->start_time_ = std::chrono::steady_clock::now();
    try {
        std::string last_failed_route;
        while (route_itr != execute_routes.end() && !SQL_SUCCEEDED(rc)) {
            std::string current_route = (*route_itr)->ToString();
            rc = (*route_itr)->Execute(stmt, this->odbc_helper_, this->status_map_);
            LOG(INFO) << "Execute route returned: " << std::to_string(rc);
            if (!SQL_SUCCEEDED(rc)) {
                this->blue_green_status_ = status_map_->Get(this->blue_green_id_);
                if (this->blue_green_status_.GetCurrentPhase().GetPhase() == BlueGreenPhase::UNKNOWN) {
                    this->end_time_ = std::chrono::steady_clock::now();
                    LOG(WARNING) << "Default execution, statuses reset, routes cleared for role: " << host_role.ToString() << ", host: " << conn_host;
                    return next_plugin->Execute(StatementHandle, StatementText, TextLength);
                }

                execute_routes = this->blue_green_status_.GetExecuteRoutes();
                route_itr = std::find_if(execute_routes.begin(), execute_routes.end(),
                    [&conn_host, &host_role](const std::shared_ptr<BaseExecuteRouting> route) {
                        return route->IsMatch(conn_host, host_role);
                    });

                if (route_itr != execute_routes.end() && (*route_itr)->ToString() == last_failed_route) {
                    route_itr = execute_routes.end();
                }
                last_failed_route = current_route;
            }
        }
        if (SQL_SUCCEEDED(rc)) {
            return rc;
        }

        LOG(WARNING) << "Default execution, out of routes: " << host_role.ToString() << ", host: " << conn_host;
        rc = next_plugin->Execute(StatementHandle, StatementText, TextLength);
    } catch (const std::exception& ex) {
        CLEAR_STMT_ERROR(stmt);
        std::string error_message("Blue/Green Execute route failed: ");
        error_message += ex.what();
        stmt->err = new ERR_INFO(error_message.c_str(), ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        rc = SQL_ERROR;
        LOG(ERROR) << error_message;
    }

    this->end_time_ = std::chrono::steady_clock::now();
    return rc;
}

int64_t BlueGreenPlugin::GetHoldTime() {
    if (this->start_time_ == std::chrono::steady_clock::time_point{}) {
        return 0;
    }
    return this->end_time_ == std::chrono::steady_clock::time_point{} ?
        (std::chrono::steady_clock::now() - this->start_time_).count()
        : (this->end_time_ - this->start_time_).count();
}

void BlueGreenPlugin::ResetRoutingTiming() {
    this->start_time_ = std::chrono::steady_clock::time_point{};
    this->end_time_ = std::chrono::steady_clock::time_point{};
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
        this->status_provider_->InitMonitoring();
    }
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
        std::map<std::string, std::string> monitoring_map;
        if (std::shared_ptr<PluginService> service = this->plugin_service_.lock()) {
            monitoring_map = service->GetOriginalConnAttr();
        }
        std::string monitoring_cluster_id = MapUtils::GetStringValue(monitoring_map, KEY_CLUSTER_ID, "<empty>");
        monitoring_cluster_id = monitoring_cluster_id + "-bg-monitor";
        monitoring_map.insert_or_assign(KEY_CLUSTER_ID, monitoring_cluster_id);
        std::shared_ptr<PluginService> monitor_plugin_service = std::make_shared<PluginService>(this->odbc_helper_->GetLibLoader(), monitoring_map);
        std::shared_ptr<BasePlugin> plugin_head = PluginChainBuilder::MonitoringBuild(monitoring_map, monitor_plugin_service);
        monitor_plugin_service->SetPluginChain(plugin_head);
        provider = std::make_shared<BlueGreenStatusProvider>(
            monitor_plugin_service,
            this->conn_attr_,
            BlueGreenPlugin::status_map_,
            this->blue_green_id_,
            this->cluster_id_);
        std::pair pair = {1, provider};
        status_providers_map_.insert_or_assign(this->blue_green_id_, pair);
        LOG(INFO) << "Created Blue Green Status Provider: " << this->blue_green_id_;
    }
    return provider;
}
