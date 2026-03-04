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

#ifndef SUBSTITUTE_CONNECT_ROUTING_H_
#define SUBSTITUTE_CONNECT_ROUTING_H_

#include "base_connect_routing.h"

#include "../../blue_green_role.h"
#include "../../blue_green_status.h"

#include "../../../../driver.h"
#include "../../../../util/odbc_helper.h"
#include "../../../../util/sliding_cache_map.h"

#include <functional>
#include <memory>
#include <string>

class SubstituteConnectRouting : public BaseConnectRouting {
public:
    SubstituteConnectRouting(
        std::string host_port,
        BlueGreenRole role,
        HostInfo substitute_info,
        std::vector<HostInfo> iam_hosts,
        std::function<void(std::string)> iam_connect_notify = nullptr)
        : substitute_info_{ substitute_info },
          iam_hosts_{ iam_hosts },
          iam_connect_notify_{ iam_connect_notify },
          BaseConnectRouting(host_port, role) { route_class_ = "SubstituteConnectRouting"; };

    SQLRETURN Connect(
        DBC* dbc,
        HostInfo info,
        std::shared_ptr<OdbcHelper> odbc_helper,
        const std::shared_ptr<SlidingCacheMap<std::string, BlueGreenStatus>> status_cache);

protected:
    HostInfo substitute_info_;
    std::vector<HostInfo> iam_hosts_;
    std::function<void(std::string)> iam_connect_notify_;
};

#endif // SUBSTITUTE_CONNECT_ROUTING_H_
