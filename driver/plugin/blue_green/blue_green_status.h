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

#ifndef BLUE_GREEN_STATUS_H_
#define BLUE_GREEN_STATUS_H_

#include "blue_green_phase.h"
#include "blue_green_role.h"

#include "../../host_info.h"

#include "../../util/concurrent_map.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

// Forward Declarations
class BaseConnectRouting;
class BaseExecuteRouting;

class BlueGreenStatus {
public:
    BlueGreenStatus();
    BlueGreenStatus(std::string id, BlueGreenPhase phase);
    BlueGreenStatus(std::string id, BlueGreenPhase phase, std::vector<std::shared_ptr<BaseConnectRouting>> connect_routes,
                    std::vector<std::shared_ptr<BaseExecuteRouting>> execute_routes,
                    std::shared_ptr<ConcurrentMap<std::string, BlueGreenRole>> role_by_host_map,
                    std::shared_ptr<ConcurrentMap<std::string, std::pair<HostInfo, HostInfo>>> corresponding_nodes);

    BlueGreenPhase GetCurrentPhase();
    std::vector<std::shared_ptr<BaseConnectRouting>> GetConnectRoutes();
    std::vector<std::shared_ptr<BaseExecuteRouting>> GetExecuteRoutes();
    std::map<std::string, BlueGreenRole> GetRoleByHosts();
    std::map<std::string, std::pair<HostInfo, HostInfo>> GetCorrespondingNodes();
    BlueGreenRole GetRole(HostInfo info);
    BlueGreenRole GetRole(std::string host);

    std::string ToString();

    bool operator==(const BlueGreenStatus& other) const {
        return blue_green_id_ == other.blue_green_id_ &&
               current_phase_ == other.current_phase_
               && connect_routes_ == other.connect_routes_
               && execute_routes_ == other.execute_routes_
               && role_by_host_map_ == other.role_by_host_map_ &&
               corresponding_nodes_ == other.corresponding_nodes_;
    }

private:
    std::string blue_green_id_;
    BlueGreenPhase current_phase_;
    std::vector<std::shared_ptr<BaseConnectRouting>> connect_routes_;
    std::vector<std::shared_ptr<BaseExecuteRouting>> execute_routes_;
    std::map<std::string, BlueGreenRole> role_by_host_map_;
    std::map<std::string, std::pair<HostInfo, HostInfo>> corresponding_nodes_;
};

#endif // BLUE_GREEN_STATUS_H_
