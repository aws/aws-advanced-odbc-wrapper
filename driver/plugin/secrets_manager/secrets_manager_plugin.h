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

#include "../base_plugin.h"

#ifndef SECRETS_MANAGER_PLUGIN_H_
#define SECRETS_MANAGER_PLUGIN_H_

class Secret {
public:
    std::string username;
    std::string password;
    std::chrono::time_point<std::chrono::system_clock> expiration_point;
};

class SecretsManagerPlugin : public BasePlugin {
public:
    SecretsManagerPlugin(DBC* dbc);
    SecretsManagerPlugin(DBC* dbc, BasePlugin* next_plugin);
    SecretsManagerPlugin(DBC* dbc, BasePlugin* next_plugin, std::shared_ptr<Aws::SecretsManager::SecretsManagerClient> client);
    ~SecretsManagerPlugin() override;

    SQLRETURN Connect(
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;
    int GetSecretsCacheSize();
    void ClearSecretsCache();

private:
    static inline const std::string SECRET_USERNAME_KEY = "username";
    static inline const std::string SECRET_PASSWORD_KEY = "password";
    static inline const std::chrono::milliseconds DEFAULT_EXPIRATION_MS = std::chrono::minutes(15);
    static inline const std::regex SECRETS_ARN_REGION_PATTERN = std::regex("^arn:aws:secretsmanager:([-\\w\\d]+):[\\d]+:secret:");
    static inline std::unordered_map<std::string, Secret> secrets_cache;
    static inline std::recursive_mutex secrets_cache_mutex;
    Secret secret;
    std::string secret_key;
    std::shared_ptr<Aws::SecretsManager::SecretsManagerClient> secrets_manager_client;
    Aws::SecretsManager::Model::GetSecretValueRequest secret_request;
    std::chrono::milliseconds expiration_ms;
    Secret ParseSecret(std::string secret_string, std::chrono::milliseconds expiration);
};

#endif // SECRETS_MANAGER_PLUGIN_H_
