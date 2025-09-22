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

#include "connection_string_keys.h"
#include "rds_strings.h"

static std::unordered_set<RDS_STR> const sensitive_key_set = {
    KEY_DB_PASSWORD,
    KEY_IDP_PASSWORD
};

static std::unordered_set<RDS_STR> const aws_odbc_key_set = {
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
};

namespace ConnectionStringHelper {
    void ParseConnectionString(std::string conn_str, std::map<std::string, std::string> &conn_map);
    RDS_STR BuildMinimumConnectionString(const std::map<RDS_STR, RDS_STR> &conn_map);
    RDS_STR BuildFullConnectionString(const std::map<RDS_STR, RDS_STR> &conn_map);
    RDS_STR MaskSensitiveInformation(const RDS_STR &conn_str);
    bool IsAwsOdbcKey(const RDS_STR &aws_odbc_key);
    bool IsSensitiveData(const RDS_STR &sensitive_key);
}

#endif // CONNECTION_STRING_HELPER_H_
