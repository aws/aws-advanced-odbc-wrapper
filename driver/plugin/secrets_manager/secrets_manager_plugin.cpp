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

SecretsManagerPlugin::SecretsManagerPlugin(DBC *dbc, BasePlugin *next_plugin, std::shared_ptr<Aws::SecretsManager::SecretsManagerClient> client) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "SECRETS_MANAGER";
    AwsSdkHelper::Init();

    auto map_end_itr = dbc->conn_attr.end();
    std::string secret_id = dbc->conn_attr.find(KEY_SECRET_ID) != map_end_itr ?
        ToStr(dbc->conn_attr.at(KEY_SECRET_ID)) : "";
    std::string region = dbc->conn_attr.find(KEY_SECRET_REGION) != map_end_itr ?
        ToStr(dbc->conn_attr.at(KEY_SECRET_REGION)) : "";
    std::string expiration_ms_str = dbc->conn_attr.find(KEY_TOKEN_EXPIRATION) != map_end_itr ?
        ToStr(dbc->conn_attr.at(KEY_TOKEN_EXPIRATION)) : "";
    std::string endpoint = dbc->conn_attr.find(KEY_SECRET_ENDPOINT) != map_end_itr ?
        ToStr(dbc->conn_attr.at(KEY_SECRET_ENDPOINT)) : "";

    std::smatch matches;
    if (std::regex_search(secret_id, matches, SECRETS_ARN_REGION_PATTERN) && matches.length() > 0) {
        region = matches[1];
    }

    if (region.empty()) {
        throw std::runtime_error("Could not determine secret region.");
    }

    if (secret_id.empty()) {
        throw std::runtime_error("Missing required parameter 'SECRET_ID'.");
    }

    secret_key = secret_id + "-" + region;

    errno = 0;
    char* end_ptr = NULL;
    int expiration_ms_int = std::strtol(expiration_ms_str.c_str(), &end_ptr, 10);
    expiration_ms = errno != 0 || *end_ptr ? DEFAULT_EXPIRATION_MS : std::chrono::milliseconds(expiration_ms_int);

    if (client) {
        secrets_manager_client = client;
    } else {
        Aws::Client::ClientConfiguration client_config;
        if (!endpoint.empty()) client_config.endpointOverride = endpoint;
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
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    SQLRETURN ret = SQL_ERROR;

    {
        std::lock_guard<std::recursive_mutex> lock_guard(secrets_cache_mutex);
        if (secrets_cache.find(secret_key) != secrets_cache.end()) {
            std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();
            Secret cached_secret = secrets_cache.at(secret_key);
            if (curr_time < cached_secret.expiration_point) {
                dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, ToRdsStr(cached_secret.username));
                dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, ToRdsStr(cached_secret.password));
                ret = next_plugin->Connect(WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
            }
        }
    }

    if (SQL_SUCCEEDED(ret)) {
        return ret;
    }

    Aws::SecretsManager::Model::GetSecretValueOutcome request_outcome = secrets_manager_client->GetSecretValue(secret_request);

    if (request_outcome.IsSuccess()) {
        Secret secret = ParseSecret(request_outcome.GetResult().GetSecretString(), expiration_ms);
        if (secret.username.empty() || secret.password.empty()) {
            if (dbc->err) delete dbc->err;
            dbc->err = new ERR_INFO("Secret did not contain username or password.", ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
            return SQL_ERROR;
        }
        {
            std::lock_guard<std::recursive_mutex> lock_guard(secrets_cache_mutex);
            secrets_cache.insert_or_assign(secret_key, secret);
        }

        dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, ToRdsStr(secret.username));
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, ToRdsStr(secret.password));
        return next_plugin->Connect(WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    } else {
        if (dbc->err) delete dbc->err;
        dbc->err = new ERR_INFO(request_outcome.GetError().GetMessage().c_str(), ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        return SQL_ERROR;
    }
}

Secret SecretsManagerPlugin::ParseSecret(std::string secret_string, std::chrono::milliseconds expiration) {
    std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();

    Aws::String json_string(secret_string);
    Aws::Utils::Json::JsonValue json_value(json_string);
    Aws::Utils::Json::JsonView view = json_value.View();

    if (view.ValueExists(SECRET_USERNAME_KEY) && view.ValueExists(SECRET_PASSWORD_KEY)) {
        return Secret{view.GetString(SECRET_USERNAME_KEY).c_str(), view.GetString(SECRET_PASSWORD_KEY).c_str(), curr_time + expiration};
    } else {
        return Secret{"", "", curr_time + expiration};
    }
}

int SecretsManagerPlugin::GetSecretsCacheSize() {
    return secrets_cache.size();
}

void SecretsManagerPlugin::ClearSecretsCache() {
    secrets_cache.clear();
}
