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

#include "blue_green_status.h"
#include "routing/connect/base_connect_routing.h"
#include "routing/execute/base_execute_routing.h"

#include <algorithm>
#include <format>
#include <sstream>

BlueGreenStatus::BlueGreenStatus() : BlueGreenStatus("<NO-ID>", BlueGreenPhase(BlueGreenPhase::UNKNOWN, false)) {}

BlueGreenStatus::BlueGreenStatus(std::string id, BlueGreenPhase phase)
    : BlueGreenStatus(id, phase, std::vector<std::shared_ptr<BaseConnectRouting>>(), std::vector<std::shared_ptr<BaseExecuteRouting>>(),
                      std::make_shared<ConcurrentMap<std::string, BlueGreenRole>>(),
                      std::make_shared<ConcurrentMap<std::string, std::pair<HostInfo, HostInfo>>>()) {}

BlueGreenStatus::BlueGreenStatus(std::string id, BlueGreenPhase phase, std::vector<std::shared_ptr<BaseConnectRouting>> connect_routes,
                                 std::vector<std::shared_ptr<BaseExecuteRouting>> execute_routes,
                                 std::shared_ptr<ConcurrentMap<std::string, BlueGreenRole>> role_by_host_map,
                                 std::shared_ptr<ConcurrentMap<std::string, std::pair<HostInfo, HostInfo>>> corresponding_nodes) {
    this->blue_green_id_ = id;
    this->current_phase_ = phase;
    this->connect_routes_ = connect_routes;
    this->execute_routes_ = execute_routes;
    this->role_by_host_map_ = role_by_host_map->GetMapCopy();
    this->corresponding_nodes_ = corresponding_nodes->GetMapCopy();
}

BlueGreenPhase BlueGreenStatus::GetCurrentPhase() {
    return this->current_phase_;
}

std::vector<std::shared_ptr<BaseConnectRouting>> BlueGreenStatus::GetConnectRoutes() {
    return this->connect_routes_;
}

std::vector<std::shared_ptr<BaseExecuteRouting>> BlueGreenStatus::GetExecuteRoutes() {
    return this->execute_routes_;
}

std::map<std::string, BlueGreenRole> BlueGreenStatus::GetRoleByHosts() {
    return this->role_by_host_map_;
}

std::map<std::string, std::pair<HostInfo, HostInfo>> BlueGreenStatus::GetCorrespondingNodes() {
    return this->corresponding_nodes_;
}

BlueGreenRole BlueGreenStatus::GetRole(HostInfo info) {
    std::string host = info.GetHost();
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return std::tolower(c); });
    return role_by_host_map_.contains(host) ? role_by_host_map_.at(host) : BlueGreenRole();
}

BlueGreenRole BlueGreenStatus::GetRole(std::string host) {
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return std::tolower(c); });
    return role_by_host_map_.contains(host) ? role_by_host_map_.at(host) : BlueGreenRole();
}

std::string BlueGreenStatus::ToString() {
    std::ostringstream role_by_host_map_str;
    std::ostringstream connect_routes_str;
    std::ostringstream execute_routes_str;

    for (const auto& [key, value] : role_by_host_map_) {
        role_by_host_map_str << key << ", " << value.ToString() << "\n";
    }

    for (auto& route : connect_routes_) {
        connect_routes_str << route->ToString() << "\n";
    }
    for (auto& route : execute_routes_) {
        execute_routes_str << route->ToString() << "\n";
    }

    return std::format(
        "bgId: {}, \n"
        "Phase: {}, \n"
        "RoleByHost: {}\n"
        "Connect Route: {}\n"
        "Execute Route: {}\n",
        this->blue_green_id_, this->current_phase_.ToString(), role_by_host_map_str.str(), connect_routes_str.str(), execute_routes_str.str());
}
