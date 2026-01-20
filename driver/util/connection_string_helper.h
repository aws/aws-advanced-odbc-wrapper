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

#ifndef CONNECTION_STRING_HELPER_H_
#define CONNECTION_STRING_HELPER_H_

#include <map>
#include <unordered_set>
#include <unordered_map>

#include "connection_string_keys.h"
#include "rds_strings.h"

static std::unordered_set<std::string> const sensitive_key_set = {
    KEY_DB_PASSWORD,
    KEY_IDP_PASSWORD
};

static std::unordered_set<std::string> const internal_wrapper_key_set = {
    KEY_RDS_TEST_CONN
};

static std::unordered_set<std::string> const aws_odbc_key_set = {
    KEY_BASE_DRIVER,
    KEY_BASE_DSN,
    // Pass DSN to avoid loading a default DSN
    KEY_DRIVER,
    KEY_AUTH_TYPE,
    KEY_EXTRA_URL_ENCODE,
    KEY_REGION,
    KEY_TOKEN_EXPIRATION,
    KEY_SECRET_ID,
    KEY_SECRET_REGION,
    KEY_SECRET_ENDPOINT,
    KEY_SECRET_USERNAME_PROPERTY,
    KEY_SECRET_PASSWORD_PROPERTY,
    KEY_IDP_ENDPOINT,
    KEY_IDP_PORT,
    KEY_IDP_USERNAME,
    KEY_IDP_PASSWORD,
    KEY_IDP_ROLE_ARN,
    KEY_IDP_SAML_ARN,
    KEY_HTTP_SOCKET_TIMEOUT,
    KEY_HTTP_CONNECT_TIMEOUT,
    KEY_RELAY_PARTY_ID,
    KEY_APP_ID,
    KEY_MFA_TYPE,
    KEY_MFA_PORT,
    KEY_MFA_TIMEOUT,
    KEY_DATABASE_DIALECT,
    KEY_HOST_SELECTOR_STRATEGY,
    KEY_ENABLE_FAILOVER,
    KEY_FAILOVER_MODE,
    KEY_ENDPOINT_TEMPLATE,
    KEY_IGNORE_TOPOLOGY_REQUEST,
    KEY_HIGH_REFRESH_RATE,
    KEY_REFRESH_RATE,
    KEY_FAILOVER_TIMEOUT,
    KEY_CLUSTER_ID,
    KEY_ENABLE_LIMITLESS,
    KEY_LIMITLESS_MODE,
    KEY_LIMITLESS_MONITOR_INTERVAL_MS,
    KEY_ROUTER_MAX_RETRIES,
    KEY_RDS_TEST_CONN
};

static std::unordered_map<std::string, std::string> const alias_to_real_map = {
    // UID
    {ALIAS_KEY_USERNAME_1, KEY_DB_USERNAME},
    {ALIAS_KEY_USERNAME_2, KEY_DB_USERNAME},
    // PWD
    {ALIAS_KEY_PASSWORD_1, KEY_DB_PASSWORD},
    {ALIAS_KEY_PASSWORD_2, KEY_DB_PASSWORD},
    {ALIAS_KEY_PASSWORD_3, KEY_DB_PASSWORD},
    // Server
    {ALIAS_KEY_SERVER_1, KEY_SERVER},
};

namespace ConnectionStringHelper {
    void ParseConnectionString(std::string conn_str, std::map<std::string, std::string> &conn_map);
    std::string BuildMinimumConnectionString(const std::map<std::string, std::string> &conn_map);
    std::string BuildFullConnectionString(const std::map<std::string, std::string> &conn_map);
    std::string MaskSensitiveInformation(const std::string &conn_str);
    std::string RemoveInternalWrapperKeys(const std::string &conn_str);
    bool IsAwsOdbcKey(const std::string &aws_odbc_key);
    bool IsSensitiveData(const std::string &sensitive_key);
    std::string GetRealKeyName(const std::string &alias_key);
}

#endif // CONNECTION_STRING_HELPER_H_
