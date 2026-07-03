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
#include "common_mock_objects.h"

#include "../../driver/plugin/federated/aws_sso_auth_plugin.h"
#include "../../driver/util/aws_sdk_helper.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/driver.h"

namespace {
    const std::string server = "host.com";
    const std::string region = "us-west-1";
    const std::string port = "1234";
    const std::string username = "abc";
    const Aws::Auth::AWSCredentials sso_credentials("access-key", "secret-key", "session-token");
    const Aws::Auth::AWSCredentials empty_credentials;
}

class AwsSsoAuthPluginTest : public testing::Test {
protected:
    std::shared_ptr<MOCK_BASE_PLUGIN> mock_base_plugin;
    std::shared_ptr<MOCK_AUTH_PROVIDER> mock_auth_provider;
    std::shared_ptr<MOCK_SSO_LOGIN_UTIL> mock_login_util;
    std::shared_ptr<MOCK_DIALECT> mock_dialect;
    std::shared_ptr<MOCK_ODBC_HELPER> mock_odbc_helper;
    DBC* dbc;

    static void SetUpTestSuite() {
        AwsSdkHelper::EnsureInitialized();
    }

    void SetUp() override {
        mock_auth_provider = std::make_shared<MOCK_AUTH_PROVIDER>();
        mock_login_util = std::make_shared<MOCK_SSO_LOGIN_UTIL>();
        mock_base_plugin = std::make_shared<MOCK_BASE_PLUGIN>();
        mock_dialect = std::make_shared<MOCK_DIALECT>();
        mock_odbc_helper = std::make_shared<MOCK_ODBC_HELPER>();
        dbc = new DBC();
        dbc->conn_attr.insert_or_assign(KEY_SERVER, server);
        dbc->conn_attr.insert_or_assign(KEY_REGION, region);
        dbc->conn_attr.insert_or_assign(KEY_PORT, port);
        dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, username);
        // Minimal SSO config so the eagerly-constructed login util does not throw
        // in tests that build a real plugin (those inject the mock util instead).
        dbc->conn_attr.insert_or_assign(KEY_SSO_START_URL, "https://my-sso.awsapps.com/start");
        dbc->conn_attr.insert_or_assign(KEY_SSO_ACCOUNT_ID, "123456789012");
        dbc->conn_attr.insert_or_assign(KEY_SSO_ROLE_NAME, "MyRole");
    }
    void TearDown() override {
        if (dbc) delete dbc;
        if (mock_auth_provider) mock_auth_provider.reset();
        if (mock_login_util) mock_login_util.reset();
    }
};

TEST_F(AwsSsoAuthPluginTest, Connect_Success_WithInjectedProvider) {
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

    AwsSsoAuthPlugin plugin(dbc, mock_base_plugin, mock_login_util, mock_auth_provider);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(token_info.first, dbc->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(AwsSsoAuthPluginTest, Connect_Success_LoginThenToken) {
    dbc->allow_interactive_auth = true;
    EXPECT_CALL(*mock_login_util, GetAwsCredentials(true, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(sso_credentials));

    AwsSsoAuthPlugin plugin(dbc, mock_base_plugin, mock_login_util, nullptr, mock_dialect, mock_odbc_helper);

    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_PROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_FALSE(dbc->conn_attr.at(KEY_DB_PASSWORD).empty());
}

TEST_F(AwsSsoAuthPluginTest, Connect_Fail_LoginReturnsNoCredentials) {
    dbc->allow_interactive_auth = false;
    EXPECT_CALL(*mock_login_util, GetAwsCredentials(false, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::DoAll(
            testing::SetArgReferee<1>("SSO login required, reconnect interactively"),
            testing::Return(empty_credentials)));
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));

    AwsSsoAuthPlugin plugin(dbc, mock_base_plugin, mock_login_util, nullptr, mock_dialect, mock_odbc_helper);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}

TEST_F(AwsSsoAuthPluginTest, Connect_Fail_MissingParams) {
    dbc->conn_attr.erase(KEY_DB_USERNAME);
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));

    AwsSsoAuthPlugin plugin(dbc, mock_base_plugin, mock_login_util, mock_auth_provider);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}

TEST_F(AwsSsoAuthPluginTest, Connect_Success_CacheExpireRetry) {
    std::pair<std::string, bool> cached_token("expired_cached_token", true);
    std::pair<std::string, bool> fresh_token("fresh_token", false);
    EXPECT_CALL(
        *mock_auth_provider,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(cached_token))
        .WillOnce(testing::Return(fresh_token));
    EXPECT_CALL(*mock_login_util, GetAwsCredentials(testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(sso_credentials));
    EXPECT_CALL(*mock_auth_provider, UpdateAwsCredential(testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return());
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(SQL_ERROR))
        .WillOnce(testing::Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_odbc_helper, GetSqlStateAndLogMessage(testing::An<DBC*>(), testing::_))
        .WillOnce(testing::Return("28000"));
    EXPECT_CALL(*mock_dialect, IsSqlStateAccessError(testing::_, testing::An<const std::string&>()))
        .WillOnce(testing::Return(true));

    AwsSsoAuthPlugin plugin(dbc, mock_base_plugin, mock_login_util, mock_auth_provider, mock_dialect, mock_odbc_helper);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(fresh_token.first, dbc->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(AwsSsoAuthPluginTest, Connect_Fail_CacheHit_NonAccessError_NoRetry) {
    std::pair<std::string, bool> cached_token("cached_token", true);
    EXPECT_CALL(
        *mock_auth_provider,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(cached_token));
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_ERROR));
    EXPECT_CALL(*mock_odbc_helper, GetSqlStateAndLogMessage(testing::An<DBC*>(), testing::_))
        .WillOnce(testing::Return("08001"));
    EXPECT_CALL(*mock_dialect, IsSqlStateAccessError(testing::_, testing::An<const std::string&>()))
        .WillOnce(testing::Return(false));

    AwsSsoAuthPlugin plugin(dbc, mock_base_plugin, mock_login_util, mock_auth_provider, mock_dialect, mock_odbc_helper);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}
