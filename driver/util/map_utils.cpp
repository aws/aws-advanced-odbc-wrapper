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

#include "map_utils.h"

#include <chrono>
#include <map>
#include <string>

#include "connection_string_keys.h"


std::string MapUtils::GetStringValue(const std::map<std::string, std::string> &map, const std::string &key, const std::string &defaultValue) {
    return map.contains(key) ? map.at(key) : defaultValue;
}

std::chrono::milliseconds MapUtils::GetMillisecondsValue(const std::map<std::string, std::string> &map, const std::string &key, const std::chrono::milliseconds &defaultValue) {
    return map.contains(key) ? std::chrono::milliseconds(static_cast<int>(std::strtol(map.at(key).c_str(), nullptr, 0))) : defaultValue;
}

bool MapUtils::GetBooleanValue(const std::map<std::string, std::string> &map, const std::string &key, const bool defaultValue) {
    return map.contains(key) ? map.at(key) == VALUE_BOOL_TRUE  : defaultValue;
}

int MapUtils::GetIntValue(const std::map<std::string, std::string> &map, const std::string &key, const int defaultValue) {
    return map.contains(key) ? static_cast<int>(std::strtol(map.at(key).c_str(), nullptr, 0))  : defaultValue;
}
