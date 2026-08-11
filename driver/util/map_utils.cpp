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
#include <optional>
#include <string>

#include "connection_string_keys.h"
#include "logger_wrapper.h"
#include "number_utils.h"

namespace {
    // Returns the parsed value, or std::nullopt (with a warning) when the key is present but its value is not a valid base-10 integer.
    std::optional<int64_t> ParseValue(const std::map<std::string, std::string> &map, const std::string &key) {
        if (!map.contains(key)) {
            return std::nullopt;
        }
        const std::string &value = map.at(key);
        const std::optional<int64_t> parsed = NumberUtils::ParseInt64(value);
        if (!parsed.has_value()) {
            LOG(WARNING) << "Invalid numeric value \"" << value << "\" for attribute " << key << "; using default";
        }
        return parsed;
    }
}  // namespace

std::string MapUtils::GetStringValue(const std::map<std::string, std::string> &map, const std::string &key, const std::string &defaultValue) {
    return map.contains(key) ? map.at(key) : defaultValue;
}

std::chrono::milliseconds MapUtils::GetMillisecondsValue(const std::map<std::string, std::string> &map, const std::string &key, const std::chrono::milliseconds &defaultValue) {
    const std::optional<int64_t> parsed = ParseValue(map, key);
    return parsed.has_value() ? std::chrono::milliseconds(parsed.value()) : defaultValue;
}

std::chrono::seconds MapUtils::GetSecondsValue(const std::map<std::string, std::string> &map, const std::string &key, const std::chrono::seconds &defaultValue) {
    const std::optional<int64_t> parsed = ParseValue(map, key);
    return parsed.has_value() ? std::chrono::seconds(parsed.value()) : defaultValue;
}

bool MapUtils::GetBooleanValue(const std::map<std::string, std::string> &map, const std::string &key, const bool defaultValue) {
    return map.contains(key) ? map.at(key) == VALUE_BOOL_TRUE  : defaultValue;
}

int MapUtils::GetIntValue(const std::map<std::string, std::string> &map, const std::string &key, const int defaultValue) {
    if (!map.contains(key)) {
        return defaultValue;
    }
    const std::string &value = map.at(key);
    const std::optional<int> parsed = NumberUtils::ParseInt(value);
    if (!parsed.has_value()) {
        LOG(WARNING) << "Invalid numeric value \"" << value << "\" for attribute " << key << "; using default";
        return defaultValue;
    }
    return parsed.value();
}
