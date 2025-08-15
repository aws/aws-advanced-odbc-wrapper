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

#include "highest_weight_host_selector.h"

#include <algorithm>
#include <stdexcept>

HostInfo HighestWeightHostSelector::GetHost(std::vector<HostInfo> hosts, bool is_writer, std::unordered_map<std::string, std::string>) {
    std::vector<HostInfo> eligible_hosts;
    eligible_hosts.reserve(hosts.size());

    std::copy_if(hosts.begin(), hosts.end(), std::back_inserter(eligible_hosts), [&is_writer](const HostInfo& host) {
        return host.IsHostUp() && is_writer == host.IsHostWriter();
    });

    if (eligible_hosts.empty()) {
        throw std::runtime_error("No eligible hosts found in list");
    }

    auto highest_weight_host = std::max_element(
        eligible_hosts.begin(),
        eligible_hosts.end(),
        [](const HostInfo& a, const HostInfo& b) {
            return a.GetWeight() < b.GetWeight();
        }
    );

    if (highest_weight_host == eligible_hosts.end()) {
        throw std::runtime_error("No eligible hosts found in list");
    }

    return *highest_weight_host;
}
