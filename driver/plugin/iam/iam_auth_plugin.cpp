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
#include "../../util/rds_utils.h"

IamAuthPlugin::IamAuthPlugin(DBC *dbc) : IamAuthPlugin(dbc, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, BasePlugin *next_plugin) : IamAuthPlugin(dbc, next_plugin, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, BasePlugin *next_plugin, const std::shared_ptr<AuthProvider>& auth_provider) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "IAM";

    if (auth_provider) {
        this->auth_provider = auth_provider;
    } else {
        std::string region = dbc->conn_attr.contains(KEY_REGION) ?
            dbc->conn_attr.at(KEY_REGION) : "";
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
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    const std::string server = dbc->conn_attr.contains(KEY_SERVER) ?
        dbc->conn_attr.at(KEY_SERVER) : "";
    const std::string iam_host = dbc->conn_attr.contains(KEY_IAM_HOST) ?
        dbc->conn_attr.at(KEY_IAM_HOST) : server;
    std::string region = dbc->conn_attr.contains(KEY_REGION) ?
        dbc->conn_attr.at(KEY_REGION) : "";
    if (region.empty()) {
        region = dbc->conn_attr.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
            : Aws::Region::US_EAST_1;
    }
    const std::string port = dbc->conn_attr.contains(KEY_PORT) ?
        dbc->conn_attr.at(KEY_PORT) : "";
    const std::string username = dbc->conn_attr.contains(KEY_DB_USERNAME) ?
        dbc->conn_attr.at(KEY_DB_USERNAME) : "";
    const std::chrono::milliseconds token_expiration = dbc->conn_attr.contains(KEY_TOKEN_EXPIRATION) ?
        std::chrono::milliseconds(std::strtol(dbc->conn_attr.at(KEY_TOKEN_EXPIRATION).c_str(), nullptr, 10)) : AuthProvider::DEFAULT_EXPIRATION_MS;

    const bool extra_url_encode = dbc->conn_attr.contains(KEY_EXTRA_URL_ENCODE) ?
        std::strtol(dbc->conn_attr.at(KEY_EXTRA_URL_ENCODE).c_str(), nullptr, 0) > 0 : false;

    if (iam_host.empty() || region.empty() || port.empty() || username.empty()) {
        CLEAR_DBC_ERROR(dbc);
        dbc->err = new ERR_INFO("Missing required parameters for IAM Authentication", ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        return SQL_ERROR;
    }

    std::pair<std::string, bool> token = auth_provider->GetToken(iam_host, region, port, username, true, extra_url_encode, token_expiration);

    SQLRETURN ret = SQL_ERROR;

    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, token.first);
    ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

    // Unsuccessful connection using cached token
    //  Skip cache and generate a new token to retry
    if (!SQL_SUCCEEDED(ret) && token.second) {
        token = auth_provider->GetToken(iam_host, region, port, username, false, extra_url_encode, token_expiration);
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, token.first);
        ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    return ret;
}
