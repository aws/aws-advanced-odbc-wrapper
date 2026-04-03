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

#include "blue_green_hasher.h"
#include "blue_green_interim_status.h"

#include <functional>

std::hash<std::string> BlueGreenInterimStatus::hasher;

BlueGreenInterimStatus::BlueGreenInterimStatus(
    BlueGreenPhase phase,
    std::string version,
    int port,
    std::vector<HostInfo> initial_topology,
    std::vector<HostInfo> current_topology,
    std::map<std::string, std::optional<std::string>> initial_ip_host_map,
    std::map<std::string, std::optional<std::string>> current_ip_host_map,
    std::set<std::string> host_names,
    bool all_start_topology_ip_changed,
    bool all_start_topology_endpoints_removed,
    bool all_topology_changed)
    : phase_{ phase },
      version_{ version },
      port_{ port },
      initial_topology_{ initial_topology },
      current_topology_{ current_topology },
      initial_ip_host_map_{ initial_ip_host_map },
      current_ip_host_map_{ current_ip_host_map },
      host_names_{ host_names },
      all_start_topology_ip_changed_{ all_start_topology_ip_changed },
      all_start_topology_endpoints_removed_{ all_start_topology_endpoints_removed },
      all_topology_changed_{ all_topology_changed } {}

int32_t BlueGreenInterimStatus::GetHashCode() {
    int32_t result = this->GetValueHash(1, this->phase_.ToString());
    result = this->GetValueHash(result, this->version_);
    result = this->GetValueHash(result, std::to_string(this->port_));
    result = this->GetValueHash(result, this->all_start_topology_ip_changed_ ? "true" : "false");
    result = this->GetValueHash(result, this->all_start_topology_endpoints_removed_ ? "true" : "false");
    result = this->GetValueHash(result, this->all_topology_changed_ ? "true" : "false");

    for (const std::string& host_name : this->host_names_) {
        result = this->GetValueHash(result, host_name);
    }
    for (const HostInfo& info : this->initial_topology_) {
        result = this->GetValueHash(result, info.GetHostPortPair() + std::to_string(info.GetHostRole()));
    }
    for (const HostInfo& info : this->current_topology_) {
        result = this->GetValueHash(result, info.GetHostPortPair() + std::to_string(info.GetHostRole()));
    }
    for (const auto& [key, value] : this->initial_ip_host_map_) {
        result = this->GetValueHash(result, key + value.value_or(""));
    }
    for (const auto& [key, value] : this->current_ip_host_map_) {
        result = this->GetValueHash(result, key + value.value_or(""));
    }

    return result;
}

int32_t BlueGreenInterimStatus::GetValueHash(int current_hash, std::string value) const {
    return current_hash * 31 + BlueGreenHasher::GetHash(value);
}
