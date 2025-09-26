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

#include "attribute_validator.h"

#include <unordered_set>
#include <map>

#include "connection_string_keys.h"
#include "rds_strings.h"

bool AttributeValidator::ShouldKeyBeInt(const RDS_STR& key) {
    static const std::unordered_set<RDS_STR> int_keys = {
        KEY_PORT,
        KEY_TOKEN_EXPIRATION,
        KEY_IAM_PORT,
        KEY_IDP_PORT,
        KEY_HTTP_SOCKET_TIMEOUT,
        KEY_HTTP_CONNECT_TIMEOUT,
        KEY_IGNORE_TOPOLOGY_REQUEST,
        KEY_HIGH_REFRESH_RATE,
        KEY_REFRESH_RATE,
        KEY_FAILOVER_TIMEOUT,
        KEY_LIMITLESS_MONITOR_INTERVAL_MS,
        KEY_ROUTER_MAX_RETRIES,
        KEY_LIMITLESS_MAX_RETRIES
    };
    return int_keys.contains(key);
}

bool AttributeValidator::IsValueInt(const RDS_STR& value) {
    if (value.empty()) return false;
    try {
        std::size_t pos{};
        std::stoi(value, &pos);
        return pos == value.length();
    } catch (std::invalid_argument) {
        return false;
    } catch (std::out_of_range) {
        return false;
    }
}

std::unordered_set<RDS_STR> AttributeValidator::ValidateMap(const std::map<RDS_STR, RDS_STR>& conn_attr) {
    std::unordered_set<RDS_STR> invalid_keys;
    for (const auto& e : conn_attr) {
        RDS_STR key = e.first;
        RDS_STR value = e.second;

        if (ShouldKeyBeInt(key) && !IsValueInt(value)) {
            invalid_keys.insert(key);
        }
    }
    return invalid_keys;
}
