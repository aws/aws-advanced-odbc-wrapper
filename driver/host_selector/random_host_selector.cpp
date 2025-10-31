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

#include "random_host_selector.h"

#include <algorithm>
#include <random>
#include <stdexcept>

HostInfo RandomHostSelector::GetHost(std::vector<HostInfo> hosts, bool is_writer,
    std::unordered_map<std::string, std::string> properties) {

    std::vector<HostInfo> selection;
    selection.reserve(hosts.size());

    std::ranges::copy_if(hosts, std::back_inserter(selection), [&is_writer](const HostInfo& host) {
        return host.IsHostUp() && (is_writer ? host.IsHostWriter() : true);
    });

    if (selection.empty()) {
        throw std::runtime_error("No available hosts found in list");
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(selection.size()) - 1);

    const int rand_idx = dis(gen);
    return selection[rand_idx];
}
