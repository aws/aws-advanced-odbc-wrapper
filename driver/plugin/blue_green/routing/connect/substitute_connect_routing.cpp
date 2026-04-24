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

#include "substitute_connect_routing.h"

#include "../../../base_plugin.h"

#include "../../../../util/connection_string_keys.h"
#include "../../../../util/logger_wrapper.h"
#include "../../../../util/rds_utils.h"
#include "../../../../util/plugin_service.h"

#include <map>

SQLRETURN SubstituteConnectRouting::Connect(
    DBC* dbc,
    HostInfo info,
    std::shared_ptr<OdbcHelper> odbc_helper,
    std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> status_cache)
{
    const std::shared_ptr<BasePlugin> plugin_head = dbc->plugin_service->GetPluginChain();
    odbc_helper->Disconnect(dbc);

    std::string substitute_host = this->substitute_info_.GetHost();
    bool const using_ip_host = RdsUtils::IsIpv4(substitute_host);
    bool const using_iam = dbc->conn_attr.contains(KEY_AUTH_TYPE) && dbc->conn_attr.at(KEY_AUTH_TYPE) == VALUE_AUTH_IAM;

    // Substitute connections
    dbc->conn_attr.insert_or_assign(KEY_SERVER, substitute_host);
    if (substitute_info_.GetPort() != HostInfo::NO_PORT) {
        dbc->conn_attr.insert_or_assign(KEY_PORT, std::to_string(substitute_info_.GetPort()));
    };

    // Connect as usual if not using IAM
    if (!using_ip_host || !using_iam) {
        SQLRETURN const rt = plugin_head->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(rt)) {
            const std::shared_ptr<Dialect> dialect = dbc->plugin_service->GetDialect();
            std::string error_message;
            const std::string sql_state = odbc_helper->GetSqlStateAndLogMessage(dbc, error_message);
            if (dialect && dialect->IsSqlStateAccessError(sql_state.c_str(), error_message)) {
                // Login error - let another routing try
                odbc_helper->Disconnect(dbc);
                return SQL_ERROR;
            }
        }
        return rt;
    }

    // IAM Host needed to generate token when connecting with IP
    if (this->iam_hosts_.empty()) {
        throw std::runtime_error("Connecting with IP address when IAM authentication is enabled requires an 'IAM_HOST' parameter.");
    }

    for (const HostInfo& iam_info : this->iam_hosts_) {
        dbc->conn_attr.insert_or_assign(KEY_IAM_HOST, iam_info.GetHost());
        if (iam_info.GetPort() != HostInfo::NO_PORT) {
            dbc->conn_attr.insert_or_assign(KEY_IAM_PORT, std::to_string(iam_info.GetPort()));
        };
        dbc->conn_attr.insert_or_assign(KEY_MONITORING_CONN_UUID, VALUE_BOOL_TRUE);
        const SQLRETURN rt = plugin_head->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        dbc->conn_attr.erase(KEY_MONITORING_CONN_UUID);
        const std::shared_ptr<Dialect> dialect = dbc->plugin_service->GetDialect();
        std::string error_message;
        const std::string sql_state = odbc_helper->GetSqlStateAndLogMessage(dbc, error_message);
        if (dialect && dialect->IsSqlStateAccessError(sql_state.c_str(), error_message)) {
            // Login/access error with this IAM host - try next IAM host
            odbc_helper->Disconnect(dbc);
            continue;
        }
        if (!SQL_SUCCEEDED(rt)) {
            // Non-login error (network, DNS, etc.) - propagate
            throw std::runtime_error("Encountered non-login error, SQL State: " + sql_state);
        }
        if (iam_connect_notify_) {
            iam_connect_notify_(iam_info.GetHost());
        }
        return rt;
    }

    // All IAM hosts exhausted with login errors
    throw std::runtime_error("Unable to open a connection to " + substitute_host + ". All IAM hosts exhausted with login errors.");
}
