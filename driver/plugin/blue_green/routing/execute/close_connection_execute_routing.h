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

#ifndef CLOSE_CONNECTION_EXECUTE_ROUTING_H_
#define CLOSE_CONNECTION_EXECUTE_ROUTING_H_

#include "base_execute_routing.h"

#include "../../blue_green_role.h"
#include "../../blue_green_status.h"

#include "../../../../driver.h"
#include "../../../../util/odbc_helper.h"
#include "../../../../util/sliding_cache_map.h"

#include <memory>
#include <string>

class CloseConnectionExecuteRouting : public BaseExecuteRouting {
public:
    CloseConnectionExecuteRouting(std::string host_port, BlueGreenRole role) : BaseExecuteRouting(host_port, role) {
        route_class_ = "CloseConnectionExecuteRouting";
    };

    SQLRETURN Execute(
        STMT* stmt,
        std::shared_ptr<OdbcHelper> odbc_helper,
        std::shared_ptr<SlidingCacheMap<std::string, BlueGreenStatus>> status_cache);
};

#endif // CLOSE_CONNECTION_EXECUTE_ROUTING_H_
