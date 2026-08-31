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

#include "auth_mock_objects.h"

#include <thread>
#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../driver/plugin/secrets_manager/secrets_manager_plugin.h"
#include "../../driver/util/aws_sdk_helper.h"
#include "../../driver/util/connection_string_keys.h"

#include "../../driver/util/windows_headers.h"
#include <sql.h>

namespace {
    const auto TEST_SECRET_STRING = "{\"username\": \"test_user\", \"password\": \"my_pwd\"}";
    const auto TEST_CUSTOM_SECRET_STRING = "{\"db_user\": \"foo\", \"db_pass\": \"bar\"}";
    const auto TEST_MIXED_SECRET_STRING = "{\"db_user\": \"foo\", \"password\": \"bar\"}";
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

Aws::SecretsManager::Model::GetSecretValueOutcome GetMockSecretValueOutcomeCustom() {
    return GetMockSecretValueOutcome(TEST_CUSTOM_SECRET_STRING);
}

Aws::SecretsManager::Model::GetSecretValueOutcome GetMockSecretValueOutcomeCustomWithFallback() {
    return GetMockSecretValueOutcome(TEST_MIXED_SECRET_STRING);
}

Aws::SecretsManager::Model::GetSecretValueOutcome GetMockSecretValueOutcomeInvalid() {
    return GetMockSecretValueOutcome(TEST_SECRET_INVALID_JSON);
}

Aws::SecretsManager::Model::GetSecretValueOutcome GetMockSecretValueOutcomeMissingCredentials() {
    return GetMockSecretValueOutcome(TEST_SECRET_WITHOUT_CRED);
}

class SecretsManagerPluginTest : public testing::Test {
protected:
    std::shared_ptr<MockBasePlugin> mock_base_plugin_;
    std::shared_ptr<MockSecretsManagerClient> mock_sm_client_;
    DBC* dbc_;

    static void SetUpTestSuite() {
        AwsSdkHelper::Init();
    }

    static void TearDownTestSuite() {
        AwsSdkHelper::Shutdown();
    }

    void SetUp() override {
        mock_sm_client_ = std::make_shared<MockSecretsManagerClient>();
        mock_base_plugin_ = std::make_shared<MockBasePlugin>();
        EXPECT_CALL(
            *mock_base_plugin_,
            Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
            .WillRepeatedly(testing::Return(SQL_SUCCESS));
        dbc_ = new DBC();
    }

    void TearDown() override {
        if (mock_sm_client_) mock_sm_client_.reset();
        if (dbc_) delete dbc_;
    }
};

TEST_F(SecretsManagerPluginTest, MissingSecretId) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_REGION, "us-east-2");

    EXPECT_CALL(*mock_sm_client_, GetSecretValue(testing::_)).Times(0);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);

    EXPECT_TRUE(dbc_->err);
    EXPECT_STREQ("Missing required parameter 'SECRET_ID'.", dbc_->err->error_msg);

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, MissingRegion) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "test-secret");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_REGION, "");

    EXPECT_CALL(*mock_sm_client_, GetSecretValue(testing::_)).Times(0);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);

    EXPECT_TRUE(dbc_->err);
    EXPECT_STREQ("Could not determine secret region.", dbc_->err->error_msg);

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, UseSecretIdAndRegion) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "test-secret");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_REGION, "us-east-2");

    EXPECT_CALL(*mock_sm_client_, GetSecretValue(testing::_)).Times(testing::Exactly(1)).WillRepeatedly(GetMockSecretValueOutcomeSuccess);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);
    SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    plugin->ClearSecretsCache();

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, UseSecretArn) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_REGION, "");

    EXPECT_CALL(
        *mock_sm_client_,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(GetMockSecretValueOutcomeSuccess);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);
    SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    plugin->ClearSecretsCache();

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, UseCachedSecret) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_REGION, "");

    EXPECT_CALL(
        *mock_sm_client_,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(GetMockSecretValueOutcomeSuccess);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);
    EXPECT_EQ(SQL_SUCCESS, plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));
    EXPECT_EQ(SQL_SUCCESS, plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));

    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    plugin->ClearSecretsCache();

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, UseExpiredSecret) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_REGION, "");
    dbc_->conn_attr.insert_or_assign(KEY_TOKEN_EXPIRATION, "1");

    EXPECT_CALL(
        *mock_sm_client_,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(2))
        .WillRepeatedly(GetMockSecretValueOutcomeSuccess);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);

    EXPECT_EQ(SQL_SUCCESS, plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(SQL_SUCCESS, plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));

    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    plugin->ClearSecretsCache();

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, SecretIsInvalid) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_REGION, "");
    dbc_->conn_attr.insert_or_assign(KEY_TOKEN_EXPIRATION, "1");

    EXPECT_CALL(
        *mock_sm_client_,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(GetMockSecretValueOutcomeInvalid);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);

    EXPECT_EQ(SQL_ERROR, plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));
    EXPECT_EQ(0, plugin->GetSecretsCacheSize());

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, SecretMissingCredentials) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_REGION, "");
    dbc_->conn_attr.insert_or_assign(KEY_TOKEN_EXPIRATION, "1");

    EXPECT_CALL(
        *mock_sm_client_,
        GetSecretValue(testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(GetMockSecretValueOutcomeMissingCredentials);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);

    EXPECT_EQ(SQL_ERROR, plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT));
    EXPECT_EQ(0, plugin->GetSecretsCacheSize());

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, UseCustomSecretUsernameKey) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_USERNAME_PROPERTY, "db_user");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_PASSWORD_PROPERTY, "db_pass");

    EXPECT_CALL(*mock_sm_client_, GetSecretValue(testing::_)).Times(testing::Exactly(1)).WillRepeatedly(GetMockSecretValueOutcomeCustom);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);
    SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    EXPECT_EQ("foo", dbc_->conn_attr[KEY_DB_USERNAME]);
    EXPECT_EQ("bar", dbc_->conn_attr[KEY_DB_PASSWORD]);
    plugin->ClearSecretsCache();

    delete plugin;
}

TEST_F(SecretsManagerPluginTest, UseCustomSecretUsernameKeyWithFallback) {
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_ID, "arn:aws:secretsmanager:us-east-2:123456789012:secret:my_secret-abcdef");
    dbc_->conn_attr.insert_or_assign(KEY_SECRET_USERNAME_PROPERTY, "db_user");

    EXPECT_CALL(*mock_sm_client_, GetSecretValue(testing::_)).Times(testing::Exactly(1)).WillRepeatedly(GetMockSecretValueOutcomeCustomWithFallback);

    SecretsManagerPlugin* plugin = new SecretsManagerPlugin(dbc_, mock_base_plugin_, mock_sm_client_);
    SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(1, plugin->GetSecretsCacheSize());
    EXPECT_EQ("foo", dbc_->conn_attr[KEY_DB_USERNAME]);
    EXPECT_EQ("bar", dbc_->conn_attr[KEY_DB_PASSWORD]);
    plugin->ClearSecretsCache();

    delete plugin;
}
