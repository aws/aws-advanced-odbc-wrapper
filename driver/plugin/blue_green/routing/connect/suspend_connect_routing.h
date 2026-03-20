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

#ifndef SUSPEND_CONNECT_ROUTING_H_
#define SUSPEND_CONNECT_ROUTING_H_

#include "base_connect_routing.h"

#include "../../blue_green_role.h"
#include "../../blue_green_status.h"

#include "../../../../driver.h"
#include "../../../../util/odbc_helper.h"
#include "../../../../util/concurrent_map.h"

#include <memory>
#include <string>

class SuspendConnectRouting : public BaseConnectRouting {
public:
    SuspendConnectRouting(
        std::string host_port,
        BlueGreenRole role,
        std::string blue_green_id)
        : blue_green_id_{ blue_green_id }, BaseConnectRouting(host_port, role) { route_class_ = "SuspendConnectRouting"; };

    SQLRETURN Connect(
        DBC* dbc,
        HostInfo info,
        std::shared_ptr<OdbcHelper> odbc_helper,
        std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> status_cache);

private:
    std::string blue_green_id_;
};

#endif // SUSPEND_CONNECT_ROUTING_H_
