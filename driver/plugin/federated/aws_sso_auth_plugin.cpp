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

#include "aws_sso_auth_plugin.h"

#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/map_utils.h"
#include "../../util/plugin_service.h"
#include "../../util/rds_utils.h"

AwsSsoAuthPlugin::AwsSsoAuthPlugin(DBC* dbc) : AwsSsoAuthPlugin(dbc, nullptr) {}

AwsSsoAuthPlugin::AwsSsoAuthPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin)
    : AwsSsoAuthPlugin(dbc, next_plugin, nullptr, nullptr, nullptr, nullptr) {}

AwsSsoAuthPlugin::AwsSsoAuthPlugin(
    DBC* dbc,
    std::shared_ptr<BasePlugin> next_plugin,
    const std::shared_ptr<SsoBrowserLoginUtil>& login_util,
    const std::shared_ptr<AuthProvider>& auth_provider,
    std::shared_ptr<Dialect> dialect,
    std::shared_ptr<OdbcHelper> odbc_helper)
    : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "AWS_SSO";

    login_util_ = login_util ? login_util : std::make_shared<SsoBrowserLoginUtil>(dbc->conn_attr);
    auth_provider_ = auth_provider;

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
}

AwsSsoAuthPlugin::~AwsSsoAuthPlugin()
{
    if (auth_provider_) {
        auth_provider_.reset();
    }
    if (login_util_) {
        login_util_.reset();
    }
}

std::string AwsSsoAuthPlugin::ResolveRegion(DBC* dbc)
{
    std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_SSO_REGION, "");
    if (region.empty()) {
        region = MapUtils::GetStringValue(dbc->conn_attr, KEY_REGION, "");
    }
    if (region.empty()) {
        region = dbc->conn_attr.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
            : Aws::Region::US_EAST_1;
    }
    return region;
}

SQLRETURN AwsSsoAuthPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    LOG(INFO) << "[" << plugin_name << "] Entering Connect";
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    const std::string server = MapUtils::GetStringValue(dbc->conn_attr, KEY_SERVER, "");
    const std::string iam_host = MapUtils::GetStringValue(dbc->conn_attr, KEY_IAM_HOST, server);
    const std::string region = ResolveRegion(dbc);
    const std::string port = AuthProvider::GetPort(dbc);
    const std::string username = MapUtils::GetStringValue(dbc->conn_attr, KEY_DB_USERNAME, "");
    const std::chrono::milliseconds token_expiration = dbc->conn_attr.contains(KEY_TOKEN_EXPIRATION) ?
        std::chrono::seconds(std::strtol(dbc->conn_attr.at(KEY_TOKEN_EXPIRATION).c_str(), nullptr, 10))
        : AuthProvider::DEFAULT_EXPIRATION_MS;
    const bool extra_url_encode = MapUtils::GetBooleanValue(dbc->conn_attr, KEY_EXTRA_URL_ENCODE, false);

    if (iam_host.empty() || region.empty() || port.empty() || username.empty()) {
        const std::string err_msg = "Missing required parameters for " + plugin_name + " Authentication";
        LOG(ERROR) << err_msg;
        CLEAR_DBC_ERROR(dbc);
        dbc->err = new ERR_INFO(err_msg.c_str(), ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        return SQL_ERROR;
    }

    // Acquire credentials via SSO if not already resolved
    // An interactive browser login is only attempted when the caller permitted prompting.
    if (!auth_provider_) {
        std::string login_error;
        const Aws::Auth::AWSCredentials credentials =
            login_util_->GetAwsCredentials(dbc->allow_interactive_auth, login_error);
        if (credentials.IsEmpty()) {
            const std::string err_msg = login_error.empty()
                ? "AWS IAM Identity Center (SSO) authentication failed" : login_error;
            LOG(ERROR) << err_msg;
            CLEAR_DBC_ERROR(dbc);
            dbc->err = new ERR_INFO(err_msg.c_str(), ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
            return SQL_ERROR;
        }
        auth_provider_ = std::make_shared<AuthProvider>(region, credentials);
    }

    std::pair<std::string, bool> token = auth_provider_->GetToken(
        iam_host, region, port, username, true, extra_url_encode, token_expiration);
    const bool is_cached_token = token.second;

    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, token.first);
    SQLRETURN ret = next_plugin->Connect(
        ConnectionHandle,
        WindowHandle,
        OutConnectionString,
        BufferLength,
        StringLengthPtr,
        DriverCompletion
    );

    if (SQL_SUCCEEDED(ret)) {
        return ret;
    }

    // Check if the failure is a login/access error.
    std::string sql_state;
    std::string error_message;
    if (odbc_helper_) {
        sql_state = odbc_helper_->GetSqlStateAndLogMessage(dbc, error_message);
    }
    const bool is_access_error = dialect_ && !sql_state.empty()
        && dialect_->IsSqlStateAccessError(sql_state.c_str(), error_message);

    // Retry with a fresh token only on a cached-token access error
    // Network/DNS errors aren't token-related, so don't retry.
    if (!is_cached_token || !is_access_error) {
        return ret;
    }

    LOG(WARNING) << "[" << plugin_name << "] Cached token failed to connect with access error (sql_state="
                 << sql_state << "). Retrying with fresh SSO credentials and token";

    // Refresh credentials through SSO
    std::string login_error;
    const Aws::Auth::AWSCredentials credentials = login_util_->GetAwsCredentials(dbc->allow_interactive_auth, login_error);
    if (credentials.IsEmpty()) {
        return ret;
    }
    auth_provider_->UpdateAwsCredential(credentials, region);

    // Then generate new RDS Token
    token = auth_provider_->GetToken(iam_host, region, port, username, false, extra_url_encode, token_expiration);
    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, token.first);
    ret = next_plugin->Connect(
        ConnectionHandle,
        WindowHandle,
        OutConnectionString,
        BufferLength,
        StringLengthPtr,
        DriverCompletion
    );

    return ret;
}
