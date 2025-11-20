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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "auth_mock_objects.h"

#include "../../driver/plugin/iam/iam_auth_plugin.h"
#include "../../driver/util/aws_sdk_helper.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/driver.h"

namespace {
    const std::string server = "host.com";
    const std::string region = "us-west-1";
    const std::string port = "1234";
    const std::string username = "abc";
}

class IamAuthPluginTest : public testing::Test {
protected:
    MOCK_BASE_PLUGIN* mock_base_plugin;
    std::shared_ptr<MOCK_AUTH_PROVIDER> mock_auth_provider;
    DBC* dbc;

    // Runs once per suite
    static void SetUpTestSuite() {
        AwsSdkHelper::Init();
    }
    static void TearDownTestSuite() {
        AwsSdkHelper::Shutdown();
    }

    // Runs per test
    void SetUp() override {
        mock_auth_provider = std::make_shared<MOCK_AUTH_PROVIDER>();
        mock_base_plugin = new MOCK_BASE_PLUGIN();
        dbc = new DBC();
        dbc->conn_attr.insert_or_assign(KEY_SERVER, server);
        dbc->conn_attr.insert_or_assign(KEY_REGION, region);
        dbc->conn_attr.insert_or_assign(KEY_PORT, port);
        dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, username);
    }
    void TearDown() override {
        // mock_base_plugin should be cleaned up by plugin chain
        if (dbc) delete dbc;
        if (mock_auth_provider) mock_auth_provider.reset();
    }
};

TEST_F(IamAuthPluginTest, Connect_MissingParam) {
    dbc->conn_attr.clear();

    IamAuthPlugin plugin(dbc, mock_base_plugin, mock_auth_provider);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
    EXPECT_EQ(dbc->err->native_err, ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
}

TEST_F(IamAuthPluginTest, Connect_Success) {
    std::pair<std::string, bool> token_info("cached_token", true);
    EXPECT_CALL(
        *mock_auth_provider,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(token_info));
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    IamAuthPlugin plugin(dbc, mock_base_plugin, mock_auth_provider);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(token_info.first, dbc->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(IamAuthPluginTest, Connect_Success_Empty_Region) {
    dbc->conn_attr.insert_or_assign(KEY_SERVER, "mydbname.cluster-xyz.us-east-2.rds.amazonaws.com");
    dbc->conn_attr.erase(KEY_REGION);
    std::pair<std::string, bool> token_info("cached_token", true);
    EXPECT_CALL(
        *mock_auth_provider,
        GetToken(testing::_, "us-east-2", testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(token_info));
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    IamAuthPlugin plugin(dbc, mock_base_plugin, mock_auth_provider);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(token_info.first, dbc->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(IamAuthPluginTest, Connect_Success_IAM_HOST) {
    std::string test_iam_host = "test-host";
    std::pair<std::string, bool> token_info("cached_token", true);
    EXPECT_CALL(
        *mock_auth_provider,
        GetToken(test_iam_host, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(token_info));
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    dbc->conn_attr.insert_or_assign(KEY_IAM_HOST, test_iam_host);
    IamAuthPlugin plugin(dbc, mock_base_plugin, mock_auth_provider);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(token_info.first, dbc->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(IamAuthPluginTest, Connect_Success_CacheExpire) {
    std::pair<std::string, bool> expired_token_info("expired_cached_token", true);
    std::pair<std::string, bool> valid_token_info("fresh_token", false);
    EXPECT_CALL(
        *mock_auth_provider,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(expired_token_info))
        .WillOnce(testing::Return(valid_token_info));
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(SQL_ERROR))
        .WillOnce(testing::Return(SQL_SUCCESS));

    IamAuthPlugin plugin(dbc, mock_base_plugin, mock_auth_provider);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(valid_token_info.first, dbc->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(IamAuthPluginTest, Connect_Fail_CacheMiss) {

    std::pair<std::string, bool> token_info("fresh_token", false);
    EXPECT_CALL(
        *mock_auth_provider,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(token_info));
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_ERROR));

    IamAuthPlugin plugin(dbc, mock_base_plugin, mock_auth_provider);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}

TEST_F(IamAuthPluginTest, Connect_Fail_CacheHit) {
    std::pair<std::string, bool> cached_token_info("cached_token", true);
    std::pair<std::string, bool> fresh_token_info("fresh_token", false);
    EXPECT_CALL(
        *mock_auth_provider,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(cached_token_info))
        .WillOnce(testing::Return(fresh_token_info));
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillRepeatedly(testing::Return(SQL_ERROR));

    IamAuthPlugin plugin(dbc, mock_base_plugin, mock_auth_provider);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}
