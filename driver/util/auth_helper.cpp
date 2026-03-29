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

#include "auth_helper.h"

#include "../dialect/dialect.h"
#include "../driver.h"
#include "connection_string_keys.h"
#include "map_utils.h"
#include "plugin_service.h"

std::string AuthHelper::GetPort(DBC* dbc) {
    std::string port = MapUtils::GetStringValue(dbc->conn_attr, KEY_IAM_PORT, "");
    if (port.empty()) {
        port = MapUtils::GetStringValue(dbc->conn_attr, KEY_PORT, "");
    }
    if (port.empty() && dbc->plugin_service) {
        std::shared_ptr<Dialect> dialect = dbc->plugin_service->GetDialect();
        if (dialect) {
            int default_port = dialect->GetDefaultPort();
            if (default_port > 0) {
                port = std::to_string(default_port);
            }
        }
    }
    return port;
}

bool AuthHelper::GetExtraUrlEncode(DBC* dbc) {
    if (dbc->conn_attr.contains(KEY_EXTRA_URL_ENCODE)) {
        return MapUtils::GetBooleanValue(dbc->conn_attr, KEY_EXTRA_URL_ENCODE, false);
    }
    // Default to true for Aurora PostgreSQL and Limitless dialects
    if (dbc->plugin_service) {
        std::shared_ptr<Dialect> dialect = dbc->plugin_service->GetDialect();
        if (dialect) {
            DatabaseDialectType dialect_type = dialect->GetDialectType();
            if (dialect_type == DatabaseDialectType::AURORA_POSTGRESQL ||
                dialect_type == DatabaseDialectType::AURORA_POSTGRESQL_LIMITLESS) {
                return true;
            }
        }
    }
    return false;
}
