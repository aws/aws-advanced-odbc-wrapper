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

#ifndef LIMITLESS_ROUTER_MONITOR_H_
#define LIMITLESS_ROUTER_MONITOR_H_
#include "../../util/windows_headers.h"

#include <sql.h>
#include <sqltypes.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../../host_info.h"

class BasePlugin;
class DialectLimitless;
class LimitlessQueryHelper;
class OdbcHelper;
class RdsLibLoader;
struct DBC;

class LimitlessRouterMonitor {
public:
    LimitlessRouterMonitor(
        std::shared_ptr<BasePlugin> plugin_head,
        const std::shared_ptr<DialectLimitless>& dialect,
        const std::shared_ptr<OdbcHelper> &odbc_helper,
        const std::shared_ptr<LimitlessQueryHelper> &limitless_query_helper);
    ~LimitlessRouterMonitor();

    void Close();
    virtual void Open(
        DBC* dbc,
        bool block_and_query_immediately,
        int host_port,
        unsigned int interval_ms
    );
    virtual bool IsStopped();

    std::vector<HostInfo> GetLimitlessRouters();
    [[nodiscard]] std::shared_ptr<RdsLibLoader> GetLibLoader() const;
    [[nodiscard]] std::shared_ptr<BasePlugin> GetPluginHead() const;

protected:
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    std::shared_ptr<std::vector<HostInfo>> limitless_routers_;
    std::mutex limitless_routers_mutex_;
    std::shared_ptr<RdsLibLoader> lib_loader_;
    std::shared_ptr<BasePlugin> plugin_head_;
    std::condition_variable monitor_loop_cv_;
    std::mutex monitor_loop_mutex_;

    std::atomic_bool stopped_ = false;
    unsigned int interval_ms_;
    std::shared_ptr<std::thread> monitor_thread_ = nullptr;
    std::shared_ptr<DialectLimitless> dialect_;
    std::shared_ptr<OdbcHelper> odbc_helper_;
    std::shared_ptr<LimitlessQueryHelper> limitless_query_helper_;

    void Run(SQLHENV henv, SQLHDBC conn, const std::map<std::string, std::string>& conn_attr, int host_port);
};

#endif // LIMITLESS_ROUTER_MONITOR_H_
