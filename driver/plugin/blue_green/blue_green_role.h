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

#ifndef BLUE_GREEN_ROLE_H_
#define BLUE_GREEN_ROLE_H_

#include <map>
#include <string>

class BlueGreenRole {
public:
    typedef enum {
        SOURCE,
        TARGET,
        UNKNOWN
    } Role;

    static inline std::map<std::string, Role> const ROLE_MAPPING_V1_0 = {
        {"BLUE_GREEN_DEPLOYMENT_SOURCE",    Role::SOURCE},
        {"BLUE_GREEN_DEPLOYMENT_TARGET",    Role::TARGET}
    };

    static inline std::map<Role, std::string> const ROLE_TO_STRING = {
        {Role::SOURCE, "SOURCE"},
        {Role::TARGET, "TARGET"}
    };

    BlueGreenRole();
    BlueGreenRole(Role role);

    Role GetRole() const;
    std::string ToString() const;

    bool operator==(const BlueGreenRole& other) const { return role_ == other.role_; }

    static BlueGreenRole ParseRole(std::string value, std::string version);

private:
    Role role_;
};

#endif // BLUE_GREEN_ROLE_H_
