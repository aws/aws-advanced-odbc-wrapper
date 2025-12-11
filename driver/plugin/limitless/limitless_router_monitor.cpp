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

#include "limitless_router_monitor.h"

#ifdef WIN32
    #include <windows.h>
#endif

#include <sqlext.h>

#include <chrono>
#include <regex>

#include "../../host_info.h"
#include "../../odbcapi_rds_helper.h"
#include "../../util/cluster_helper.h"
#include "../../util/connection_string_helper.h"
#include "../../util/logger_wrapper.h"
#include "limitless_query_helper.h"

LimitlessRouterMonitor::LimitlessRouterMonitor(
    BasePlugin* plugin_head,
    const std::shared_ptr<DialectLimitless>& dialect,
    const std::shared_ptr<OdbcHelper> &odbc_helper_,
    const std::shared_ptr<LimitlessQueryHelper> &limitless_query_helper_) {
    this->plugin_head_ = plugin_head;
    this->dialect_ = dialect;
    this->odbc_helper_ = odbc_helper_;
    this->limitless_query_helper_ = limitless_query_helper_;
}

LimitlessRouterMonitor::~LimitlessRouterMonitor() {
    this->Close();
    this->limitless_routers_ = nullptr;
}

void LimitlessRouterMonitor::Open(
    DBC* dbc,
    const bool block_and_query_immediately,
    int host_port,
    const unsigned int interval_ms
) {
    this->interval_ms_ = interval_ms;
    this->limitless_routers_ = std::make_shared<std::vector<HostInfo>>();
    this->lib_loader_ = dbc->env->driver_lib_loader;

    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC local_hdbc = SQL_NULL_HANDLE;

    // Open a new connection
    odbc_helper_->AllocEnv(&henv);
    ENV* env = static_cast<ENV*>(henv);
    const RdsLibResult res = odbc_helper_->BaseAllocEnv(env);
    odbc_helper_->SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    env->driver_lib_loader = lib_loader_;

    if (block_and_query_immediately) {
        LOG(INFO) << "Attempting to query routers on initial setup";
        odbc_helper_->AllocDbc(henv, local_hdbc);
        DBC* local_dbc = static_cast<DBC*>(local_hdbc);
        local_dbc->conn_attr = dbc->conn_attr;

        const SQLRETURN rc = plugin_head_->Connect(
            local_hdbc,
            nullptr,
            nullptr,
            0,
            nullptr,
            SQL_DRIVER_NOPROMPT);
        if (SQL_SUCCEEDED(rc)) {
            const std::vector<HostInfo> new_limitless_routers = this->limitless_query_helper_->QueryForLimitlessRouters(local_hdbc, host_port, dialect_);
            const std::lock_guard<std::mutex> guard(this->limitless_routers_mutex_);
            *(this->limitless_routers_) = new_limitless_routers;
            LOG(INFO) << "Queried routers on initial setup: " << new_limitless_routers.size();
        } else {
            // Not successful, ensure limitless routers is empty.
            const std::lock_guard<std::mutex> guard(this->limitless_routers_mutex_);
            this->limitless_routers_->clear();
            LOG(WARNING) << "Failed to query routers on initial setup";
        }
    }

    // Start monitoring thread; if block_and_query_immediately is false, then local_hdbc is SQL_NULL_HANDLE, and the thread will connect after the monitor interval has passed.
    this->monitor_thread_ = std::make_shared<std::thread>(&LimitlessRouterMonitor::Run, this, henv, local_hdbc, dbc->conn_attr, host_port);
}

bool LimitlessRouterMonitor::IsStopped() {
    return this->stopped_;
}

void LimitlessRouterMonitor::Close() {
    if (this->stopped_) {
        return;
    }

    this->stopped_ = true;
    this->monitor_loop_cv_.notify_all();

    if (this->monitor_thread_ != nullptr) {
        this->monitor_thread_->join();
        this->monitor_thread_ = nullptr;
    }
}

void LimitlessRouterMonitor::Run(SQLHENV henv, SQLHDBC conn, const std::map<std::string, std::string>& conn_attr, int host_port) {
    DBC* dbc = static_cast<DBC*>(conn);
    do {
        std::unique_lock lock(monitor_loop_mutex_);
        monitor_loop_cv_.wait_for(lock, std::chrono::milliseconds(this->interval_ms_));

        // If monitor_loop_cv_ has been notified, skip the rest of the loop.
        if (this->stopped_) {
            break;
        }

        if (conn == SQL_NULL_HANDLE || GetNodeId(conn, dialect_).empty()) {
            if (conn) {
                odbc_helper_->DisconnectAndFree(&conn);
                conn = SQL_NULL_HANDLE;
            }

            odbc_helper_->AllocDbc(henv, conn);
            dbc = static_cast<DBC*>(conn);
            dbc->conn_attr = conn_attr;

            const SQLRETURN rc = plugin_head_->Connect(
                conn,
                nullptr,
                nullptr,
                0,
                nullptr,
                SQL_DRIVER_NOPROMPT);
            if (!SQL_SUCCEEDED(rc)) {
                odbc_helper_->DisconnectAndFree(&conn);
                conn = SQL_NULL_HANDLE;

                // wait the full interval and then try to reconnect
                LOG(WARNING) << "Limitless Monitor failed to connect to an instance";
                continue;
            } // else, connection was successful, proceed below
        }

        const std::vector<HostInfo> new_limitless_routers = this->limitless_query_helper_->QueryForLimitlessRouters(conn, host_port, dialect_);
        // LimitlessQueryHelper::QueryForLimitlessRouters will return an empty vector on an error
        // if it was a connection error, then the next loop will catch it and attempt to reconnect
        if (!new_limitless_routers.empty()) {
            LOG(WARNING) << "Limitless Monitor failed to query any routers";
            const std::lock_guard<std::mutex> guard(this->limitless_routers_mutex_);
            *(this->limitless_routers_) = new_limitless_routers;
        }
    } while (!this->stopped_);

    if (conn != SQL_NULL_HANDLE) {
        odbc_helper_->DisconnectAndFree(&conn);
        conn = SQL_NULL_HANDLE;
    }
    odbc_helper_->FreeEnv(&henv);
}
