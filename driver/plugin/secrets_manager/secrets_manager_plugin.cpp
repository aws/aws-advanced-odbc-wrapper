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

SecretsManagerPlugin::SecretsManagerPlugin(DBC *dbc) : SecretsManagerPlugin(dbc, nullptr) {}

SecretsManagerPlugin::SecretsManagerPlugin(DBC *dbc, BasePlugin *next_plugin) : SecretsManagerPlugin(dbc, next_plugin, nullptr) {}

SecretsManagerPlugin::SecretsManagerPlugin(DBC *dbc, BasePlugin *next_plugin, const std::shared_ptr<Aws::SecretsManager::SecretsManagerClient>& client) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "SECRETS_MANAGER";

    const std::string secret_id = dbc->conn_attr.contains(KEY_SECRET_ID) ?
        dbc->conn_attr.at(KEY_SECRET_ID) : "";
    std::string region = dbc->conn_attr.contains(KEY_SECRET_REGION) ?
        dbc->conn_attr.at(KEY_SECRET_REGION) : "";
    const std::string endpoint = dbc->conn_attr.contains(KEY_SECRET_ENDPOINT) ?
        dbc->conn_attr.at(KEY_SECRET_ENDPOINT) : "";

    expiration_ms = dbc->conn_attr.contains(KEY_TOKEN_EXPIRATION) ?
        std::chrono::seconds(std::strtol(dbc->conn_attr.at(KEY_TOKEN_EXPIRATION).c_str(), nullptr, 0)) : DEFAULT_EXPIRATION_MS;

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
    SQLRETURN ret = SQL_ERROR;
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    {
        const std::lock_guard<std::recursive_mutex> lock_guard(secrets_cache_mutex);
        if (secrets_cache.contains(secret_key)) {
            LOG(INFO) << "Found existing token in cache";
            const std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();
            const Secret cached_secret = secrets_cache.at(secret_key);
            if (curr_time < cached_secret.expiration_point) {
                dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, cached_secret.username);
                dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, cached_secret.password);
                ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
            } else {
                LOG(INFO) << "Existing token expired";
                secrets_cache.erase(secret_key);
            }
        }
    }

    if (SQL_SUCCEEDED(ret)) {
        return ret;
    }

    Aws::SecretsManager::Model::GetSecretValueOutcome request_outcome = secrets_manager_client->GetSecretValue(secret_request);

    if (request_outcome.IsSuccess()) {
        const Secret secret = ParseSecret(request_outcome.GetResult().GetSecretString(), expiration_ms);
        if (secret.username.empty() || secret.password.empty()) {
            LOG(ERROR) << "Secrets Manager did not return DB credentials.";
            CLEAR_DBC_ERROR(dbc);
            dbc->err = new ERR_INFO("Secret did not contain username or password.", ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
            return SQL_ERROR;
        }
        {
            const std::lock_guard<std::recursive_mutex> lock_guard(secrets_cache_mutex);
            secrets_cache.insert_or_assign(secret_key, secret);
        }

        dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, secret.username);
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, secret.password);
        return next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    } else {
        LOG(ERROR) << "Failed to get secrets from Secrets Manager.";
        CLEAR_DBC_ERROR(dbc);
        const std::string fail_msg = "Failed to obtain secrets with error: [" + request_outcome.GetError().GetMessage() + "]";
    dbc->err = new ERR_INFO(fail_msg.c_str(), ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        return SQL_ERROR;
    }
}

Secret SecretsManagerPlugin::ParseSecret(const std::string &secret_string, const std::chrono::milliseconds expiration) {
    const std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();

    const Aws::Utils::Json::JsonValue json_value(secret_string);
    const Aws::Utils::Json::JsonView view = json_value.View();

    if (view.ValueExists(SECRET_USERNAME_KEY) && view.ValueExists(SECRET_PASSWORD_KEY)) {
        return Secret{
            .username = view.GetString(SECRET_USERNAME_KEY),
            .password = view.GetString(SECRET_PASSWORD_KEY),
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
