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

#ifndef BLUE_GREEN_INTERIM_STATUS_H_
#define BLUE_GREEN_INTERIM_STATUS_H_

#include "blue_green_phase.h"
#include "blue_green_role.h"

#include "../../host_info.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class BlueGreenInterimStatus {
public:
    BlueGreenInterimStatus() = default;
    BlueGreenInterimStatus(
        BlueGreenPhase phase,
        std::string version,
        int port,
        std::vector<HostInfo> initial_topology,
        std::vector<HostInfo> current_topology,
        std::map<std::string, std::string> initial_ip_host_map,
        std::map<std::string, std::string> current_ip_host_map,
        std::set<std::string> host_names,
        bool all_start_topology_ip_changed,
        bool all_start_topology_endpoints_removed,
        bool all_topology_changed
    );

    int GetHashCode();

    BlueGreenPhase phase_ = BlueGreenPhase::UNKNOWN;
    std::string version_;
    int port_;
    std::vector<HostInfo> initial_topology_;
    std::vector<HostInfo> current_topology_;
    std::map<std::string, std::string> initial_ip_host_map_;
    std::map<std::string, std::string> current_ip_host_map_;
    std::set<std::string> host_names_;
    bool all_start_topology_ip_changed_;
    bool all_start_topology_endpoints_removed_;
    bool all_topology_changed_;

protected:
    int GetValueHash(int current_hash, std::string value) const;
    static std::hash<std::string> hasher;
};

#endif // BLUE_GREEN_INTERIM_STATUS_H_
