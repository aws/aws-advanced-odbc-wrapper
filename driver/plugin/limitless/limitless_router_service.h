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

#ifndef LIMITLESS_ROUTER_SERVICE_H_
#define LIMITLESS_ROUTER_SERVICE_H_

#include "limitless_router_monitor.h"
#include "limitless_query_helper.h"
#include "../../host_info.h"
#include "../../host_selector/round_robin_host_selector.h"
#include "../../host_selector/highest_weight_host_selector.h"
#include "../../util/sliding_cache_map.h"
#include "../../util/odbc_helper.h"
#include "../../dialect/dialect.h"

typedef enum {
    MONITOR_INTERVAL_MS = 7500,
    CONNECT_RETRY_ATTEMPTS = 5
} LimitlessDefault;

class LimitlessRouterService {
public:
    LimitlessRouterService(
        const std::shared_ptr<DialectLimitless> &dialect,
        const std::map<std::string, std::string> &conn_attr,
        const std::shared_ptr<OdbcHelper> &odbc_helper,
        const std::shared_ptr<LimitlessQueryHelper> &limitless_query_helper);

    ~LimitlessRouterService();
    RoundRobinHostSelector round_robin_;
    HighestWeightHostSelector highest_weight_;

    virtual std::shared_ptr<LimitlessRouterMonitor> CreateMonitor(
        const std::map<std::string, std::string>& conn_attr,
        BasePlugin* plugin_head,
        DBC* dbc,
        const std::shared_ptr<DialectLimitless>& dialect) const;
    virtual SQLRETURN EstablishConnection(BasePlugin* plugin_head, DBC* dbc);
    virtual void StartMonitoring(DBC* dbc, const std::shared_ptr<DialectLimitless> &dialect);

    static std::unordered_map<std::string, std::pair<unsigned int, std::shared_ptr<LimitlessRouterMonitor>>> limitless_router_monitors_;

private:
    static std::mutex limitless_router_monitors_mutex_;
    std::string router_monitor_key_;
    std::shared_ptr<DialectLimitless> dialect_;
    int limitless_monitor_interval_ms_;
    int max_router_retries_;
    int max_connect_retries_;
    int host_port_;
    std::shared_ptr<OdbcHelper> odbc_helper_;
    std::shared_ptr<LimitlessQueryHelper> limitless_query_helper_;
};

#endif // LIMITLESS_ROUTER_SERVICE_H_
