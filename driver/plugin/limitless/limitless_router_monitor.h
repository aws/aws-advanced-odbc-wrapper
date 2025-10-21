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

#ifdef WIN32
    #include <windows.h>
#endif

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>

#include "../../host_info.h"
#include "../../util/rds_strings.h"
#include "../../dialect/dialect.h"
#include "../base_plugin.h"

class LimitlessRouterMonitor {
public:
    LimitlessRouterMonitor(BasePlugin* plugin_head, const std::shared_ptr<DialectLimitless> &dialect);
    ~LimitlessRouterMonitor();

    void Close();
    virtual void Open(
        DBC* dbc,
        bool block_and_query_immediately,
        int host_port,
        unsigned int interval_ms
    );
    virtual bool IsStopped();

    std::shared_ptr<std::vector<HostInfo>> limitless_routers_;
    std::mutex limitless_routers_mutex_;
    std::shared_ptr<RdsLibLoader> lib_loader_;
    BasePlugin* plugin_head_;
    std::condition_variable monitor_loop_cv_;
    std::mutex monitor_loop_mutex_;

protected:
    std::atomic_bool stopped_ = false;
    unsigned int interval_ms_;
    std::shared_ptr<std::thread> monitor_thread_ = nullptr;
    std::shared_ptr<DialectLimitless> dialect_;

    void Run(SQLHENV henv, SQLHDBC conn, const std::map<RDS_STR, RDS_STR> &conn_attr, int host_port);
};

#endif // LIMITLESS_ROUTER_MONITOR_H_
