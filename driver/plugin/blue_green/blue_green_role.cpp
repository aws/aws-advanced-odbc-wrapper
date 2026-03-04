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

#include "blue_green_role.h"

#include <algorithm>
#include <map>
#include <string>

BlueGreenRole::BlueGreenRole() : BlueGreenRole(BlueGreenRole::UNKNOWN) {}

BlueGreenRole::BlueGreenRole(Role role) {
    this->role_ = role;
}

BlueGreenRole::Role BlueGreenRole::GetRole() const {
    return role_;
}

std::string BlueGreenRole::ToString() const {
    auto itr = BlueGreenRole::ROLE_TO_STRING.find(role_);
    if (itr != BlueGreenRole::ROLE_TO_STRING.end()) {
        return itr->second;
    }
    return {};
}

BlueGreenRole BlueGreenRole::ParseRole(std::string value, std::string version) {
    BlueGreenRole::Role role = UNKNOWN;

    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::toupper(c); });
    if (version == "1.0") {
        auto itr = BlueGreenRole::ROLE_MAPPING_V1_0.find(value);
        if (itr != BlueGreenRole::ROLE_MAPPING_V1_0.end()) {
            role = itr->second;
        }
    }

    return {role};
}
