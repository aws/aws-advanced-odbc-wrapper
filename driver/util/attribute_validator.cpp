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

#include <map>
#include <unordered_set>

#include "connection_string_keys.h"
#include "rds_strings.h"

bool AttributeValidator::ShouldKeyBeUnsignedInt(const RDS_STR& key) {
    static const std::unordered_set<RDS_STR> INTEGER_KEYS = {
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
    return INTEGER_KEYS.contains(key);
}

bool AttributeValidator::IsValueUnsignedInt(const RDS_STR& value) {
    if (value.empty()) {
        return false;
    }

    try {
        std::size_t pos{};
        const int int_val = std::stoi(value, &pos);
        return pos == value.length() && int_val >= 0;
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::out_of_range&) {
        return false;
    }
}

std::unordered_set<RDS_STR> AttributeValidator::ValidateMap(const std::map<RDS_STR, RDS_STR>& conn_attr) {
    std::unordered_set<RDS_STR> invalid_keys;
    for (const auto& e : conn_attr) {
        const RDS_STR key = e.first;
        const RDS_STR value = e.second;

        if (ShouldKeyBeUnsignedInt(key) && !IsValueUnsignedInt(value)) {
            invalid_keys.insert(key);
        }
    }
    return invalid_keys;
}
