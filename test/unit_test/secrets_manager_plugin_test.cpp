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

#include "mock_objects.h"

#include <thread>
#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../driver/plugin/secrets_manager/secrets_manager_plugin.h"
#include "../../driver/util/aws_sdk_helper.h"
#include "../../driver/util/connection_string_keys.h"

#ifdef WIN32
    #include <windows.h>
#endif
#include <sql.h>

namespace {
    const auto TEST_SECRET_STRING = "{\"username\": \"test_user\", \"password\": \"my_pwd\"}";
    const auto TEST_SECRET_INVALID_JSON = "invalid json";
    const auto TEST_SECRET_WITHOUT_CRED = "{\"key\": \"password\"}";
}

Aws::SecretsManager::Model::GetSecretValueOutcome GetMockSecretValueOutcome(std::string secret_string) {
    const auto expected_result = Aws::SecretsManager::Model::GetSecretValueResult().WithSecretString(secret_string);
    const auto expected_outcome = Aws::SecretsManager::Model::GetSecretValueOutcome(expected_result);
    return expected_outcome;
}

Aws::SecretsManager::Model::GetSecretValueOutcome GetMockSecretValueOutcomeSuccess() {
    return GetMockSecretValueOutcome(TEST_SECRET_STRING);
}

Aws::SecretsManager::Model::GetSecretValueOutcome GetMockecretValueOutcomeInvalid() {
    return GetMockSecretValueOutcome(TEST_SECRET_INVALID_JSON);
}

Aws::SecretsManager::Model::GetSecretValueOutcome GetMockSecretValueOutcomeMissingCredentials() {
    return GetMockSecretValueOutcome(TEST_SECRET_WITHOUT_CRED);
}

class SecretsManagerPluginTest : public testing::Test {
protected:
    std::shared_ptr<MOCK_BASE_PLUGIN> mock_base_plugin;
    std::shared_ptr<MOCK_SECRETS_MANAGER_CLIENT> mock_sm_client;
    DBC* dbc;

    static void SetUpTestSuite() {
        AwsSdkHelper::Init();
    }

    static void TearDownTestSuite() {
        AwsSdkHelper::Shutdown();
    }

    void SetUp() override {
        mock_sm_client = std::make_shared<MOCK_SECRETS_MANAGER_CLIENT>();
        mock_base_plugin = std::make_shared<MOCK_BASE_PLUGIN>();
        EXPECT_CALL(
            *mock_base_plugin,
            Connect(testing::_, testing::_, testing::_, testing::_, testing::_))
            .WillRepeatedly(testing::Return(SQL_SUCCESS));
        dbc = new DBC();
    }

    void TearDown() override {
        if (mock_base_plugin) mock_base_plugin.reset();
        if (mock_sm_client) mock_sm_client.reset();
        if (dbc) delete dbc;
    }
};

TEST_F(SecretsManagerPluginTest, MissingSecretId) {
    dbc->conn_attr.insert_or_assign(KEY_SECRET_ID, ToRdsStr(""));
    dbc->conn_attr.insert_or_assign(KEY_SECRET_REGION, ToRdsStr("us-east-2"));

    EXPECT_CALL(*mock_sm_client, GetSecretValue(testing::_)).Times(0);

    EXPECT_THROW({
        try
        {
            SecretsManagerPlugin *plugin = new SecretsManagerPlugin(dbc, mock_base_plugin.get(), mock_sm_client);
        }
        catch (const std::runtime_error e)
        {
            EXPECT_STREQ("Missing required parameter 'SECRET_ID'.", e.what());
            throw e;
        }
    }, std::runtime_error);
}

TEST_F(SecretsManagerPluginTest, MissingRegion) {
    dbc->conn_attr.insert_or_assign(KEY_SECRET_ID, ToRdsStr("test-secret"));
    dbc->conn_attr.insert_or_assign(KEY_SECRET_REGION, ToRdsStr(""));

    EXPECT_CALL(*mock_sm_client, GetSecretValue(testing::_)).Times(0);

    EXPECT_THROW({
        try
        {
            SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc, mock_base_plugin.get(), mock_sm_client);
        }
        catch (const std::runtime_error e)
        {
            EXPECT_STREQ("Could not determine secret region.", e.what());
            throw e;
        }
    }, std::runtime_error);
}

TEST_F(SecretsManagerPluginTest, UseSecretIdAndRegion) {
    dbc->conn_attr.insert_or_assign(KEY_SECRET_ID, ToRdsStr("test-secret"));
    dbc->conn_attr.insert_or_assign(KEY_SECRET_REGION, ToRdsStr("us-east-2"));

    EXPECT_CALL(*mock_sm_client, GetSecretValue(testing::_)).Times(testing::Exactly(1)).WillRepeatedly(GetMockSecretValueOutcomeSuccess);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc, mock_base_plugin.get(), mock_sm_client);
    SQLRETURN ret = plugin->Connect(nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    plugin->ClearSecretsCache();

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, UseSecretArn) {
    dbc->conn_attr.insert_or_assign(KEY_SECRET_ID, ToRdsStr("arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef"));
    dbc->conn_attr.insert_or_assign(KEY_SECRET_REGION, ToRdsStr(""));

    EXPECT_CALL(
        *mock_sm_client,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(GetMockSecretValueOutcomeSuccess);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc, mock_base_plugin.get(), mock_sm_client);
    SQLRETURN ret = plugin->Connect(nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    plugin->ClearSecretsCache();

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, UseCachedSecret) {
    dbc->conn_attr.insert_or_assign(KEY_SECRET_ID, ToRdsStr("arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef"));
    dbc->conn_attr.insert_or_assign(KEY_SECRET_REGION, ToRdsStr(""));

    EXPECT_CALL(
        *mock_sm_client,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(GetMockSecretValueOutcomeSuccess);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc, mock_base_plugin.get(), mock_sm_client);
    EXPECT_EQ(SQL_SUCCESS, plugin->Connect(nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));
    EXPECT_EQ(SQL_SUCCESS, plugin->Connect(nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));

    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    plugin->ClearSecretsCache();

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, UseExpiredSecret) {
    dbc->conn_attr.insert_or_assign(KEY_SECRET_ID, ToRdsStr("arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef"));
    dbc->conn_attr.insert_or_assign(KEY_SECRET_REGION, ToRdsStr(""));
    dbc->conn_attr.insert_or_assign(KEY_TOKEN_EXPIRATION, ToRdsStr("1"));

    EXPECT_CALL(
        *mock_sm_client,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(2))
        .WillRepeatedly(GetMockSecretValueOutcomeSuccess);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc, mock_base_plugin.get(), mock_sm_client);

    EXPECT_EQ(SQL_SUCCESS, plugin->Connect(nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(SQL_SUCCESS, plugin->Connect(nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));

    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    plugin->ClearSecretsCache();

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, SecretIsInvalid) {
    dbc->conn_attr.insert_or_assign(KEY_SECRET_ID, ToRdsStr("arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef"));
    dbc->conn_attr.insert_or_assign(KEY_SECRET_REGION, ToRdsStr(""));
    dbc->conn_attr.insert_or_assign(KEY_TOKEN_EXPIRATION, ToRdsStr("1"));

    EXPECT_CALL(
        *mock_sm_client,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(GetMockecretValueOutcomeInvalid);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc, mock_base_plugin.get(), mock_sm_client);

    EXPECT_EQ(SQL_ERROR, plugin->Connect(nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));
    EXPECT_EQ(0, plugin->GetSecretsCacheSize());

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, SecretMissingCredentials) {
    dbc->conn_attr.insert_or_assign(KEY_SECRET_ID, ToRdsStr("arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef"));
    dbc->conn_attr.insert_or_assign(KEY_SECRET_REGION, ToRdsStr(""));
    dbc->conn_attr.insert_or_assign(KEY_TOKEN_EXPIRATION, ToRdsStr("1"));

    EXPECT_CALL(
        *mock_sm_client,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(GetMockSecretValueOutcomeMissingCredentials);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc, mock_base_plugin.get(), mock_sm_client);

    EXPECT_EQ(SQL_ERROR, plugin->Connect(nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));
    EXPECT_EQ(0, plugin->GetSecretsCacheSize());

    delete plugin;
}
