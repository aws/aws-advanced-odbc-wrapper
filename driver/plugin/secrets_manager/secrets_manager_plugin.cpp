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

#include <aws/secretsmanager/SecretsManagerClient.h>
#include <aws/secretsmanager/model/GetSecretValueRequest.h>

#include "secrets_manager_plugin.h"

#include "../../util/aws_sdk_helper.h"
#include "../../util/connection_string_keys.h"
#include "../../util/map_utils.h"

SecretsManagerPlugin::SecretsManagerPlugin(DBC *dbc) : SecretsManagerPlugin(dbc, nullptr) {}

SecretsManagerPlugin::SecretsManagerPlugin(DBC *dbc, BasePlugin *next_plugin) : SecretsManagerPlugin(dbc, next_plugin, nullptr) {}

SecretsManagerPlugin::SecretsManagerPlugin(DBC *dbc, BasePlugin *next_plugin, const std::shared_ptr<Aws::SecretsManager::SecretsManagerClient>& client) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "SECRETS_MANAGER";

    const std::string secret_id = MapUtils::GetStringValue(dbc->conn_attr, KEY_SECRET_ID, "");
    std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_SECRET_REGION, "");
    const std::string endpoint = MapUtils::GetStringValue(dbc->conn_attr, KEY_SECRET_ENDPOINT, "");

    expiration_ms = MapUtils::GetMillisecondsValue(dbc->conn_attr, KEY_TOKEN_EXPIRATION, DEFAULT_EXPIRATION_MS);

    username_key = MapUtils::GetStringValue(dbc->conn_attr, KEY_SECRET_USERNAME_PROPERTY, DEFAULT_SECRET_USERNAME_KEY);
    password_key = MapUtils::GetStringValue(dbc->conn_attr, KEY_SECRET_PASSWORD_PROPERTY, DEFAULT_SECRET_PASSWORD_KEY);

    if (username_key.empty() || password_key.empty()) {
        throw std::runtime_error("SECRET_USERNAME_PROPERTY and SECRET_PASSWORD_PROPERTY cannot be empty strings. Please review the values set and ensure they match the values in the Secret value.");
    }

    if (std::smatch matches; std::regex_search(secret_id, matches, SECRETS_ARN_REGION_PATTERN) && !matches.empty()) {
        region = matches[1];
    }

    if (region.empty()) {
        throw std::runtime_error("Could not determine secret region.");
    }

    if (secret_id.empty()) {
        throw std::runtime_error("Missing required parameter 'SECRET_ID'.");
    }

    secret_key = secret_id + "-" + region;

    AwsSdkHelper::Init();
    if (client) {
        secrets_manager_client = client;
    } else {
        Aws::Client::ClientConfiguration client_config;
        if (!endpoint.empty()) {
            client_config.endpointOverride = endpoint;
        }
        client_config.region = region;
        secrets_manager_client = std::make_shared<Aws::SecretsManager::SecretsManagerClient>(client_config);
    }

    secret_request = Aws::SecretsManager::Model::GetSecretValueRequest{};
    secret_request.SetSecretId(secret_id);
}

SecretsManagerPlugin::~SecretsManagerPlugin()
{
    AwsSdkHelper::Shutdown();
}

SQLRETURN SecretsManagerPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    LOG(INFO) << "Entering Connect";
    SQLRETURN ret = SQL_ERROR;
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    {
        const std::lock_guard<std::recursive_mutex> lock_guard(secrets_cache_mutex);
        if (secrets_cache.contains(secret_key)) {
            LOG(INFO) << "Found secrets in cache";
            const std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();
            const Secret cached_secret = secrets_cache.at(secret_key);
            if (curr_time < cached_secret.expiration_point) {
                dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, cached_secret.username);
                dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, cached_secret.password);
                ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
            } else {
                LOG(INFO) << "Existing secrets are expired";
                secrets_cache.erase(secret_key);
            }
        }
    }

    if (SQL_SUCCEEDED(ret)) {
        return ret;
    }

    Aws::SecretsManager::Model::GetSecretValueOutcome request_outcome = secrets_manager_client->GetSecretValue(secret_request);

    if (request_outcome.IsSuccess()) {
        const Secret secret = ParseSecret(request_outcome.GetResult().GetSecretString(), username_key, password_key, expiration_ms);
        if (secret.username.empty() || secret.password.empty()) {
            const std::string fail_msg = "Secrets Manager did not return any database credentials, please verify the values set via SECRET_USERNAME_PROPERTY and SECRET_PASSWORD_PROPERTY and ensure they match the values in the Secret value.";
            LOG(ERROR) << fail_msg;
            CLEAR_DBC_ERROR(dbc);
            dbc->err = new ERR_INFO(fail_msg.c_str(), ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
            return SQL_ERROR;
        }
        {
            const std::lock_guard<std::recursive_mutex> lock_guard(secrets_cache_mutex);
            secrets_cache.insert_or_assign(secret_key, secret);
        }

        dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, secret.username);
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, secret.password);
        return next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }
    LOG(ERROR) << "Failed to get secrets from Secrets Manager.";
    CLEAR_DBC_ERROR(dbc);
    const std::string fail_msg = "Failed to obtain secrets with error: [" + request_outcome.GetError().GetMessage() + "]";
    dbc->err = new ERR_INFO(fail_msg.c_str(), ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
    return SQL_ERROR;
}

Secret SecretsManagerPlugin::ParseSecret(const std::string &secret_string, const std::string &username_key, const std::string &password_key, const std::chrono::milliseconds expiration) {
    const std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();

    const Aws::Utils::Json::JsonValue json_value(secret_string);
    const Aws::Utils::Json::JsonView view = json_value.View();

    if (view.ValueExists(username_key) && view.ValueExists(password_key)) {
        return Secret{
            .username = view.GetString(username_key),
            .password = view.GetString(password_key),
            .expiration_point = curr_time + expiration
        };
    }
    return Secret{
        .username = "",
        .password = "",
        .expiration_point = curr_time + expiration
    };
}

size_t SecretsManagerPlugin::GetSecretsCacheSize() {
    return secrets_cache.size();
}

void SecretsManagerPlugin::ClearSecretsCache() {
    secrets_cache.clear();
}
