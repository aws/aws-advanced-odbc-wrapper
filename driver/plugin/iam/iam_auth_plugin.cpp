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
#include "../../util/rds_utils.h"

IamAuthPlugin::IamAuthPlugin(DBC *dbc) : IamAuthPlugin(dbc, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, BasePlugin *next_plugin) : IamAuthPlugin(dbc, next_plugin, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, BasePlugin *next_plugin, const std::shared_ptr<AuthProvider>& auth_provider) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "IAM";

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
    std::string port = MapUtils::GetStringValue(dbc->conn_attr, KEY_IAM_PORT, "");
    if (port.empty()) {
        port = MapUtils::GetStringValue(dbc->conn_attr, KEY_PORT, "");
    }
    const std::string username = MapUtils::GetStringValue(dbc->conn_attr, KEY_DB_USERNAME, "");
    const std::chrono::milliseconds token_expiration = dbc->conn_attr.contains(KEY_TOKEN_EXPIRATION) ?
        std::chrono::seconds(std::strtol(dbc->conn_attr.at(KEY_TOKEN_EXPIRATION).c_str(), nullptr, 10)) : AuthProvider::DEFAULT_EXPIRATION_MS;
    const bool extra_url_encode = MapUtils::GetBooleanValue(dbc->conn_attr, KEY_EXTRA_URL_ENCODE, false);

    if (iam_host.empty() || region.empty() || port.empty() || username.empty()) {
        LOG(ERROR) << "Missing required parameters for IAM Authentication";
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
        LOG(WARNING) << "Cached token failed to connect. Retrying with fresh token";
        token = auth_provider->GetToken(iam_host, region, port, username, false, extra_url_encode, token_expiration);
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, token.first);
        ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    return ret;
}
