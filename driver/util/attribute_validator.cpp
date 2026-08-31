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
#include <optional>
#include <unordered_set>

#include "connection_string_keys.h"
#include "number_utils.h"
#include "rds_strings.h"

bool AttributeValidator::ShouldKeyBeUnsignedInt(const std::string& key) {
    static const std::unordered_set<std::string> INTEGER_KEYS = {
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
        KEY_LIMITLESS_MAX_RETRIES,
        KEY_MFA_PORT,
        KEY_MFA_TIMEOUT,
        KEY_LISTEN_PORT,
        KEY_IDP_RESPONSE_TIMEOUT,
        KEY_SSO_LISTEN_PORT,
        KEY_SSO_IDP_RESPONSE_TIMEOUT,
        KEY_CACHED_READER_KEEP_ALIVE_TIMEOUT_MS,
        KEY_SRW_CONN_TIMEOUT_MS,
        KEY_SRW_CONN_INTERVAL_MS,
        KEY_INITIAL_CONNECTION_RETRY_INTERVAL_MS,
        KEY_INITIAL_CONNECTION_RETRY_TIMEOUT_MS,
        KEY_CUSTOM_ENDPOINT_INTERVAL_MS,
        KEY_CUSTOM_ENDPOINT_MAX_INTERVAL_MS,
        KEY_CUSTOM_ENDPOINT_BACKOFF_RATE,
        KEY_WAIT_FOR_CUSTOM_ENDPOINT_INFO_TIMEOUT_MS,
        KEY_BG_CONNECT_TIMEOUT_MS,
        KEY_BG_BASELINE_REFRESH_MS,
        KEY_BG_INCREASED_REFRESH_MS,
        KEY_BG_HIGH_REFRESH_MS,
        KEY_BG_SWITCH_TIMEOUT_MS,
    };
    return INTEGER_KEYS.contains(key);
}

bool AttributeValidator::IsValueUnsignedInt(const std::string& value) {
    const std::optional<int> int_val = NumberUtils::ParseInt(value);
    return int_val.has_value() && int_val.value() >= 0;
}

std::unordered_set<std::string> AttributeValidator::ValidateMap(const std::map<std::string, std::string>& conn_attr) {
    std::unordered_set<std::string> invalid_keys;
    for (const auto& e : conn_attr) {
        const std::string key = e.first;
        const std::string value = e.second;

        if (ShouldKeyBeUnsignedInt(key) && !IsValueUnsignedInt(value)) {
            invalid_keys.insert(key);
        }
    }
    return invalid_keys;
}
