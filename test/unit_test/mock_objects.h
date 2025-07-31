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

#ifndef __MOCKOBJECTS_H__
#define __MOCKOBJECTS_H__

#include <aws/secretsmanager/SecretsManagerClient.h>
#include "gmock/gmock.h"

#include "../../driver/plugin/base_plugin.h"

class MOCK_SECRETS_MANAGER_CLIENT : public Aws::SecretsManager::SecretsManagerClient {
public:
    MOCK_SECRETS_MANAGER_CLIENT(): Aws::SecretsManager::SecretsManagerClient() {};

    MOCK_METHOD(Aws::SecretsManager::Model::GetSecretValueOutcome, GetSecretValue, (const Aws::SecretsManager::Model::GetSecretValueRequest&), (const));
};

class MockBasePlugin : public BasePlugin {
public:
    MockBasePlugin();
    ~MockBasePlugin() override;

    SQLRETURN Connect(
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;
};

#endif /* __MOCKOBJECTS_H__ */
