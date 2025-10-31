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

#include "limitless_router_service.h"

#include <chrono>

#include "../../util/logger_wrapper.h"
#include "../../util/rds_utils.h"
#include "../../util/sliding_cache_map.h"

#include "limitless_query_helper.h"
#include "limitless_router_monitor.h"

// Initialize static members
std::mutex LimitlessRouterService::limitless_router_monitors_mutex_;
SlidingCacheMap<std::string, std::pair<unsigned int, std::shared_ptr<LimitlessRouterMonitor>>> LimitlessRouterService::limitless_router_monitors;

LimitlessRouterService::LimitlessRouterService(const std::shared_ptr<DialectLimitless> &dialect, const std::map<RDS_STR, RDS_STR> &conn_attr) {
    this->dialect_ = dialect;
    this->limitless_monitor_interval_ms_ = conn_attr.contains(KEY_LIMITLESS_MONITOR_INTERVAL_MS) ?
        static_cast<int>(std::strtol(ToStr(conn_attr.at(KEY_LIMITLESS_MONITOR_INTERVAL_MS)).c_str(), nullptr, 0)) :
        LimitlessDefault::MONITOR_INTERVAL_MS;
    this->max_router_retries_ = conn_attr.contains(KEY_ROUTER_MAX_RETRIES) ?
        static_cast<int>(std::strtol(ToStr(conn_attr.at(KEY_ROUTER_MAX_RETRIES)).c_str(), nullptr, 0)) :
        LimitlessDefault::CONNECT_RETRY_ATTEMPTS;
    this->max_connect_retries_ = conn_attr.contains(KEY_LIMITLESS_MAX_RETRIES) ?
        static_cast<int>(std::strtol(ToStr(conn_attr.at(KEY_LIMITLESS_MAX_RETRIES)).c_str(), nullptr, 0)) :
        LimitlessDefault::CONNECT_RETRY_ATTEMPTS;
    this->host_port_ = conn_attr.contains(KEY_PORT) ?
        static_cast<int>(std::strtol(ToStr(conn_attr.at(KEY_PORT)).c_str(), nullptr, 0)) :
        dialect_->GetDefaultPort();
}

LimitlessRouterService::~LimitlessRouterService() {
    const std::lock_guard<std::mutex> lock_guard(limitless_router_monitors_mutex_);
    std::pair<unsigned int, std::shared_ptr<LimitlessRouterMonitor>> pair = limitless_router_monitors.Get(router_monitor_key_);
    if (pair.first == 1) {
        limitless_router_monitors.Delete(router_monitor_key_);
    } else {
        pair.first--;
        limitless_router_monitors.Put(router_monitor_key_, pair);
    }
}

std::shared_ptr<LimitlessRouterMonitor> LimitlessRouterService::CreateMonitor(
    const std::map<RDS_STR, RDS_STR> &conn_attr,
    BasePlugin* plugin_head,
    DBC* dbc,
    const std::shared_ptr<DialectLimitless>& dialect) const
{
    std::shared_ptr<LimitlessRouterMonitor> monitor = std::make_shared<LimitlessRouterMonitor>(plugin_head, dialect);

    const std::string host = conn_attr.contains(KEY_SERVER) ? ToStr(conn_attr.at(KEY_SERVER)) : "";
    std::string service_id = RdsUtils::GetRdsClusterId(host);

    if (service_id.empty()) {
        service_id = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        LOG(INFO) << "No service ID provided and could not parse service ID from host: " << host << ". Generated random service ID: " << service_id;
    }

    bool block_and_query_immediately = true;
    std::string limitless_mode = conn_attr.contains(KEY_LIMITLESS_MODE) ?
        ToStr(conn_attr.at(KEY_LIMITLESS_MODE))
        : ToStr(VALUE_LIMITLESS_MODE_IMMEDIATE);
    RDS_STR_UPPER(limitless_mode)
    if (limitless_mode == ToStr(VALUE_LIMITLESS_MODE_LAZY)) {
        block_and_query_immediately = false;
    }

    monitor->Open(
        dbc,
        block_and_query_immediately,
        host_port_,
        limitless_monitor_interval_ms_);
    return monitor;
}

SQLRETURN LimitlessRouterService::EstablishConnection(BasePlugin* next_plugin, DBC* dbc)
{
    if (dbc == nullptr) {
        return SQL_INVALID_HANDLE;
    }

    std::shared_ptr<LimitlessRouterMonitor> monitor;
    std::vector<HostInfo> limitless_routers;
    {
        monitor = limitless_router_monitors.Get(router_monitor_key_).second;
        const std::lock_guard<std::mutex> limitless_routers_guard(monitor->limitless_routers_mutex_);
        limitless_routers = *monitor->limitless_routers_;
    }

    if (limitless_routers.empty()) {
        int retry_count = -1; // Start at -1 since the first try is not a retry.

        SQLHENV henv = SQL_NULL_HANDLE;
        RDS_AllocEnv(&henv);
        ENV* env = static_cast<ENV*>(henv);
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(monitor->lib_loader_, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
            SQL_HANDLE_ENV, nullptr, &env->wrapped_env
        );
        if (!SQL_SUCCEEDED(res.fn_result)) {
            CLEAR_DBC_ERROR(dbc);
            dbc->err = new ERR_INFO("Failed to allocate ENV handle", ERR_SQLALLOCHANDLE_ON_SQL_HANDLE_ENV_FAILED);
            return SQL_ERROR;
        }
        NULL_CHECK_CALL_LIB_FUNC(monitor->lib_loader_, RDS_FP_SQLSetEnvAttr, RDS_STR_SQLSetEnvAttr,
            env->wrapped_env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0
        );
        env->driver_lib_loader = monitor->lib_loader_;

        do {
            // Query for routers directly
            SQLHDBC local_hdbc = SQL_NULL_HANDLE;

            // Open a new connection
            RDS_AllocDbc(henv, &local_hdbc);
            DBC* local_dbc = static_cast<DBC*>(local_hdbc);

            local_dbc->conn_attr = dbc->conn_attr;

            const SQLRETURN res = monitor->plugin_head_->Connect(
                local_hdbc,
                nullptr,
                nullptr,
                0,
                nullptr,
                SQL_DRIVER_NOPROMPT);

            if (SQL_SUCCEEDED(res)) {
                // LimitlessQueryHelper::QueryForLimitlessRouters will return an empty vector on an error
                // if it was a connection error, then the next loop will catch it and attempt to reconnect
                limitless_routers = LimitlessQueryHelper::QueryForLimitlessRouters(local_hdbc, host_port_, dialect_);
            }

            if (local_dbc->wrapped_dbc) {
                NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
                    local_dbc->wrapped_dbc
                );
            }
            RDS_FreeConnect(local_hdbc);

            if (limitless_routers.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(limitless_monitor_interval_ms_));
            }
            retry_count++;
        } while (limitless_routers.empty() && retry_count < max_router_retries_);
        RDS_FreeEnv(henv);
    }

    if (limitless_routers.empty()) {
        CLEAR_DBC_ERROR(dbc);
        dbc->err = new ERR_INFO("The limitless connection plugin was unable to find any limitless routers.", ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        return SQL_ERROR;
    }

    std::unordered_map<std::string, std::string> properties;

    SQLRETURN rc = SQL_ERROR;
    try {
        RoundRobinHostSelector::SetRoundRobinWeight(limitless_routers, properties);
        const HostInfo host_info = this->round_robin_.GetHost(limitless_routers, true, properties);
        dbc->conn_attr.insert_or_assign(KEY_SERVER, ToRdsStr(host_info.GetHost()));
        rc = next_plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    } catch (std::runtime_error& error) {
        LOG(INFO) << "Got runtime error while getting round robin host for limitless (trying for highest weight host next): " << error.what();
        // Proceed and attempt to connect to highest weight host.
    }

    if (SQL_SUCCEEDED(rc)) {
        // The round robin host successfully connected.
        return rc;
    }

    // Retries going by order of least loaded (highest weight).
    for (int i = 0; i < max_connect_retries_; i++) {
        try {
            const HostInfo host_info = this->highest_weight_.GetHost(limitless_routers, true, properties);
            dbc->conn_attr.insert_or_assign(KEY_SERVER, ToRdsStr(host_info.GetHost()));

            const SQLRETURN rc = next_plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
            if (SQL_SUCCEEDED(rc)) {
                // The highest weight host successfully connected.
                return rc;
            }

            // Update this host in hosts list to have down state so it's not selected again.
            for (HostInfo& host_in_list : limitless_routers) {
                if (host_in_list.GetHost() == host_info.GetHost()) {
                    host_in_list.SetHostState(DOWN);
                }
            }
        } catch (std::runtime_error &error) {
            // No more hosts.
            LOG(INFO) << "Got runtime error while getting highest weight host for limitless (no host found): " << error.what();
            break;
        }
    }

    CLEAR_DBC_ERROR(dbc);
    dbc->err = new ERR_INFO("The limitless connection plugin was unable to establish a connection.", ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
    return SQL_ERROR;
}

void LimitlessRouterService::StartMonitoring(DBC* dbc, const std::shared_ptr<DialectLimitless> &dialect)
{
    BasePlugin* plugin_head= dbc->plugin_head;
    const std::map<RDS_STR, RDS_STR> conn_attr = dbc->conn_attr;
    const std::string host = ToStr(conn_attr.at(KEY_SERVER));
    router_monitor_key_ = host;
    if (RdsUtils::IsRdsDns(host)) {
        router_monitor_key_ = RdsUtils::GetRdsClusterId(host);
    }

    const std::lock_guard<std::mutex> lock_guard(limitless_router_monitors_mutex_);
    if (!limitless_router_monitors.Find(router_monitor_key_)) {
        const std::shared_ptr<LimitlessRouterMonitor> monitor = CreateMonitor(conn_attr, plugin_head, dbc, dialect);
        limitless_router_monitors.Put(router_monitor_key_, {1, monitor});
    } else {
        std::pair<unsigned int, std::shared_ptr<LimitlessRouterMonitor>> pair = limitless_router_monitors.Get(router_monitor_key_);
        // If the monitor exists, increment the reference count.
        pair.first++;
        limitless_router_monitors.Put(router_monitor_key_, pair);
    }
}
