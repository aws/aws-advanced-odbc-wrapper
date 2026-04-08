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

#include "iam_auth_plugin.h"

#include "../../util/auth_provider.h"

#include "../../driver.h"
#include "../../util/aws_sdk_helper.h"
#include "../../util/connection_string_keys.h"
#include "../../util/map_utils.h"
#include "../../util/plugin_service.h"
#include "../../util/rds_utils.h"

IamAuthPlugin::IamAuthPlugin(DBC *dbc) : IamAuthPlugin(dbc, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin) : IamAuthPlugin(dbc, next_plugin, nullptr, nullptr, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin, const std::shared_ptr<AuthProvider>& auth_provider)
    : IamAuthPlugin(dbc, next_plugin, auth_provider, nullptr, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin,
                             const std::shared_ptr<AuthProvider>& auth_provider,
                             std::shared_ptr<Dialect> dialect,
                             std::shared_ptr<OdbcHelper> odbc_helper) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "IAM";

    if (dialect) {
        this->dialect_ = dialect;
    } else if (dbc->plugin_service) {
        this->dialect_ = dbc->plugin_service->GetDialect();
    }

    if (odbc_helper) {
        this->odbc_helper_ = odbc_helper;
    } else if (dbc->plugin_service) {
        this->odbc_helper_ = dbc->plugin_service->GetOdbcHelper();
    }

    if (auth_provider) {
        this->auth_provider = auth_provider;
    } else {
        std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_REGION, "");

        if (region.empty()) {
            region = dbc->conn_attr.contains(KEY_SERVER) ?
                RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
                : Aws::Region::US_EAST_1;
        }
        this->auth_provider = std::make_shared<AuthProvider>(region);
    }
}

IamAuthPlugin::~IamAuthPlugin()
{
    if (auth_provider) {
        auth_provider.reset();
    }
}

SQLRETURN IamAuthPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    LOG(INFO) << "Entering Connect";
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    const std::string server = MapUtils::GetStringValue(dbc->conn_attr, KEY_SERVER, "");
    const std::string iam_host = MapUtils::GetStringValue(dbc->conn_attr, KEY_IAM_HOST, server);
    std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_REGION, "");
    if (region.empty()) {
        region = dbc->conn_attr.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
            : Aws::Region::US_EAST_1;
    }
    const std::string port = AuthProvider::GetPort(dbc);
    const std::string username = MapUtils::GetStringValue(dbc->conn_attr, KEY_DB_USERNAME, "");
    const std::chrono::milliseconds token_expiration = dbc->conn_attr.contains(KEY_TOKEN_EXPIRATION) ?
        std::chrono::seconds(std::strtol(dbc->conn_attr.at(KEY_TOKEN_EXPIRATION).c_str(), nullptr, 10)) : AuthProvider::DEFAULT_EXPIRATION_MS;
    const bool extra_url_encode = MapUtils::GetBooleanValue(dbc->conn_attr, KEY_EXTRA_URL_ENCODE, false);

    if (!ValidateRequiredParams(dbc, iam_host, region, port, username)) {
        return SQL_ERROR;
    }

    std::pair<std::string, bool> token = auth_provider->GetToken(iam_host, region, port, username, true, extra_url_encode, token_expiration);
    const bool is_cached_token = token.second;

    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, token.first);
    SQLRETURN ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

    if (SQL_SUCCEEDED(ret)) {
        return ret;
    }

    // Check if the failure is a login/access error
    std::string sql_state;
    std::string error_message;
    if (odbc_helper_) {
        sql_state = odbc_helper_->GetSqlStateAndLogMessage(dbc, &error_message);
    }

    // Only retry with a fresh token if the token was from cache AND the error is an access/login error.
    // Non-access errors (network, DNS, etc.) are not token-related and should not trigger a retry.
    if (const bool is_access_error = dialect_ && !sql_state.empty() && dialect_->IsSqlStateAccessError(sql_state.c_str(), error_message);
        !is_cached_token || !is_access_error) {
        return ret;
    }

    LOG(WARNING) << "Cached token failed to connect with access error (sql_state=" << sql_state << "). Retrying with fresh token";
    token = auth_provider->GetToken(iam_host, region, port, username, false, extra_url_encode, token_expiration);
    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, token.first);
    ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

    return ret;
}

bool IamAuthPlugin::ValidateRequiredParams(DBC* dbc, const std::string& iam_host, const std::string& region,
                                           const std::string& port, const std::string& username)
{
    std::vector<std::string> missing_params;

    if (iam_host.empty()) {
        missing_params.push_back("IAM host (set '" + std::string(KEY_SERVER) + "' or '" + std::string(KEY_IAM_HOST) + "')");
    }
    if (region.empty()) {
        missing_params.push_back("Region (set '" + std::string(KEY_REGION) + "')");
    }
    if (port.empty()) {
        missing_params.push_back("Port (set '" + std::string(KEY_PORT) + "')");
    }
    if (username.empty()) {
        missing_params.push_back("Username (set '" + std::string(KEY_DB_USERNAME) + "')");
    }

    if (!missing_params.empty()) {
        std::string error_msg = "IAM Authentication failed. Missing required parameters: ";
        for (size_t i = 0; i < missing_params.size(); ++i) {
            if (i > 0) {
                error_msg += ", ";
            }
            error_msg += missing_params[i];
        }
        LOG(ERROR) << error_msg;
        CLEAR_DBC_ERROR(dbc);
        dbc->err = new ERR_INFO(error_msg.c_str(), ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        return false;
    }

    return true;
}
