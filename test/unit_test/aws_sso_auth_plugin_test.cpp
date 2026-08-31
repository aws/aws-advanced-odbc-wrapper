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
    const std::string SERVER = "host.com";
    const std::string REGION = "us-west-1";
    const std::string PORT = "1234";
    const std::string USERNAME = "abc";
    const Aws::Auth::AWSCredentials SSO_CREDENTIALS("access-key", "secret-key", "session-token");
    const Aws::Auth::AWSCredentials EMPTY_CREDENTIALS;
}

class AwsSsoAuthPluginTest : public testing::Test {
protected:
    std::shared_ptr<MockBasePlugin> mock_base_plugin_;
    std::shared_ptr<MockAuthProvider> mock_auth_provider_;
    std::shared_ptr<MockSsoLoginUtil> mock_login_util_;
    std::shared_ptr<MockDialect> mock_dialect_;
    std::shared_ptr<MockOdbcHelper> mock_odbc_helper_;
    DBC* dbc_;

    static void SetUpTestSuite() {
        AwsSdkHelper::Init();
    }
    static void TearDownTestSuite() {
        AwsSdkHelper::Shutdown();
    }

    void SetUp() override {
        mock_auth_provider_ = std::make_shared<MockAuthProvider>();
        mock_login_util_ = std::make_shared<MockSsoLoginUtil>();
        mock_base_plugin_ = std::make_shared<MockBasePlugin>();
        mock_dialect_ = std::make_shared<MockDialect>();
        mock_odbc_helper_ = std::make_shared<MockOdbcHelper>();
        dbc_ = new DBC();
        dbc_->conn_attr.insert_or_assign(KEY_SERVER, SERVER);
        dbc_->conn_attr.insert_or_assign(KEY_REGION, REGION);
        dbc_->conn_attr.insert_or_assign(KEY_PORT, PORT);
        dbc_->conn_attr.insert_or_assign(KEY_DB_USERNAME, USERNAME);
        // Minimal SSO config so the eagerly-constructed login util does not throw
        // in tests that build a real plugin (those inject the mock util instead).
        dbc_->conn_attr.insert_or_assign(KEY_SSO_START_URL, "https://my-sso.awsapps.com/start");
        dbc_->conn_attr.insert_or_assign(KEY_SSO_ACCOUNT_ID, "123456789012");
        dbc_->conn_attr.insert_or_assign(KEY_SSO_ROLE_NAME, "MyRole");
    }
    void TearDown() override {
        if (dbc_) delete dbc_;
        if (mock_auth_provider_) mock_auth_provider_.reset();
        if (mock_login_util_) mock_login_util_.reset();
    }
};

TEST_F(AwsSsoAuthPluginTest, Connect_Success_WithInjectedProvider) {
    std::pair<std::string, bool> token_info("cached_token", true);
    EXPECT_CALL(
        *mock_auth_provider_,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(token_info));
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    AwsSsoAuthPlugin plugin(dbc_, mock_base_plugin_, mock_login_util_, mock_auth_provider_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(token_info.first, dbc_->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(AwsSsoAuthPluginTest, Connect_Success_LoginThenToken) {
    dbc_->allow_interactive_auth = true;
    EXPECT_CALL(*mock_login_util_, GetAwsCredentials(true, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SSO_CREDENTIALS));

    AwsSsoAuthPlugin plugin(dbc_, mock_base_plugin_, mock_login_util_, nullptr, mock_dialect_, mock_odbc_helper_);

    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_PROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_FALSE(dbc_->conn_attr.at(KEY_DB_PASSWORD).empty());
}

TEST_F(AwsSsoAuthPluginTest, Connect_Fail_LoginReturnsNoCredentials) {
    dbc_->allow_interactive_auth = false;
    EXPECT_CALL(*mock_login_util_, GetAwsCredentials(false, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::DoAll(
            testing::SetArgReferee<1>("SSO login required, reconnect interactively"),
            testing::Return(EMPTY_CREDENTIALS)));
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));

    AwsSsoAuthPlugin plugin(dbc_, mock_base_plugin_, mock_login_util_, nullptr, mock_dialect_, mock_odbc_helper_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}

TEST_F(AwsSsoAuthPluginTest, Connect_Fail_MissingParams) {
    dbc_->conn_attr.erase(KEY_DB_USERNAME);
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));

    AwsSsoAuthPlugin plugin(dbc_, mock_base_plugin_, mock_login_util_, mock_auth_provider_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}

TEST_F(AwsSsoAuthPluginTest, Connect_Success_CacheExpireRetry) {
    std::pair<std::string, bool> cached_token("expired_cached_token", true);
    std::pair<std::string, bool> fresh_token("fresh_token", false);
    EXPECT_CALL(
        *mock_auth_provider_,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(cached_token))
        .WillOnce(testing::Return(fresh_token));
    EXPECT_CALL(*mock_login_util_, GetAwsCredentials(testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SSO_CREDENTIALS));
    EXPECT_CALL(*mock_auth_provider_, UpdateAwsCredential(testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return());
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(SQL_ERROR))
        .WillOnce(testing::Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_odbc_helper_, GetSqlStateAndLogMessage(testing::An<DBC*>(), testing::_))
        .WillOnce(testing::Return("28000"));
    EXPECT_CALL(*mock_dialect_, IsSqlStateAccessError(testing::_, testing::An<const std::string&>()))
        .WillOnce(testing::Return(true));

    AwsSsoAuthPlugin plugin(dbc_, mock_base_plugin_, mock_login_util_, mock_auth_provider_, mock_dialect_, mock_odbc_helper_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(fresh_token.first, dbc_->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(AwsSsoAuthPluginTest, Connect_Fail_CacheHit_NonAccessError_NoRetry) {
    std::pair<std::string, bool> cached_token("cached_token", true);
    EXPECT_CALL(
        *mock_auth_provider_,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(cached_token));
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_ERROR));
    EXPECT_CALL(*mock_odbc_helper_, GetSqlStateAndLogMessage(testing::An<DBC*>(), testing::_))
        .WillOnce(testing::Return("08001"));
    EXPECT_CALL(*mock_dialect_, IsSqlStateAccessError(testing::_, testing::An<const std::string&>()))
        .WillOnce(testing::Return(false));

    AwsSsoAuthPlugin plugin(dbc_, mock_base_plugin_, mock_login_util_, mock_auth_provider_, mock_dialect_, mock_odbc_helper_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}
