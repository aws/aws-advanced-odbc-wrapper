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

#include "../../driver/plugin/iam/iam_auth_plugin.h"
#include "../../driver/util/aws_sdk_helper.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/driver.h"

namespace {
    const std::string SERVER = "host.com";
    const std::string REGION = "us-west-1";
    const std::string PORT = "1234";
    const std::string USERNAME = "abc";
}

class IamAuthPluginTest : public testing::Test {
protected:
    std::shared_ptr<MockBasePlugin> mock_base_plugin_;
    std::shared_ptr<MockAuthProvider> mock_auth_provider_;
    std::shared_ptr<MockDialect> mock_dialect_;
    std::shared_ptr<MockOdbcHelper> mock_odbc_helper_;
    DBC* dbc_;

    // Runs once per suite
    static void SetUpTestSuite() {
        AwsSdkHelper::Init();
    }
    static void TearDownTestSuite() {
        AwsSdkHelper::Shutdown();
    }

    // Runs per test
    void SetUp() override {
        mock_auth_provider_ = std::make_shared<MockAuthProvider>();
        mock_base_plugin_ = std::make_shared<MockBasePlugin>();
        mock_dialect_ = std::make_shared<MockDialect>();
        mock_odbc_helper_ = std::make_shared<MockOdbcHelper>();
        dbc_ = new DBC();
        dbc_->conn_attr.insert_or_assign(KEY_SERVER, SERVER);
        dbc_->conn_attr.insert_or_assign(KEY_REGION, REGION);
        dbc_->conn_attr.insert_or_assign(KEY_PORT, PORT);
        dbc_->conn_attr.insert_or_assign(KEY_DB_USERNAME, USERNAME);
    }
    void TearDown() override {
        if (dbc_) delete dbc_;
        if (mock_auth_provider_) mock_auth_provider_.reset();
    }
};

TEST_F(IamAuthPluginTest, Connect_MissingParam) {
    dbc_->conn_attr.clear();

    IamAuthPlugin plugin(dbc_, mock_base_plugin_, mock_auth_provider_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
    EXPECT_EQ(dbc_->err->native_err, ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
}

TEST_F(IamAuthPluginTest, Connect_SSO_CredentialFail) {
    mock_auth_provider_->mock_has_resolved_credentials = false;
    dbc_->conn_attr.insert_or_assign(KEY_AWS_PROFILE, "sso-test");

    EXPECT_CALL(*mock_auth_provider_, GetToken(testing::_, testing::_, testing::_, testing::_,
        testing::_, testing::_, testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(*mock_base_plugin_, Connect(testing::_, testing::_, testing::_, testing::_,
        testing::_, testing::_)).Times(testing::Exactly(0));

    IamAuthPlugin plugin(dbc_, mock_base_plugin_, mock_auth_provider_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
    EXPECT_EQ(dbc_->err->native_err, ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
    EXPECT_NE(std::string(dbc_->err->error_msg).find("aws sso login"), std::string::npos);
}

TEST_F(IamAuthPluginTest, Connect_Success) {
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

    IamAuthPlugin plugin(dbc_, mock_base_plugin_, mock_auth_provider_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(token_info.first, dbc_->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(IamAuthPluginTest, Connect_Success_Empty_Region) {
    dbc_->conn_attr.insert_or_assign(KEY_SERVER, "mydbname.cluster-xyz.us-east-2.rds.amazonaws.com");
    dbc_->conn_attr.erase(KEY_REGION);
    std::pair<std::string, bool> token_info("cached_token", true);
    EXPECT_CALL(
        *mock_auth_provider_,
        GetToken(testing::_, "us-east-2", testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(token_info));
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    IamAuthPlugin plugin(dbc_, mock_base_plugin_, mock_auth_provider_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(token_info.first, dbc_->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(IamAuthPluginTest, Connect_Success_IAM_HOST) {
    std::string test_iam_host = "test-host";
    std::pair<std::string, bool> token_info("cached_token", true);
    EXPECT_CALL(
        *mock_auth_provider_,
        GetToken(test_iam_host, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(token_info));
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    dbc_->conn_attr.insert_or_assign(KEY_IAM_HOST, test_iam_host);
    IamAuthPlugin plugin(dbc_, mock_base_plugin_, mock_auth_provider_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(token_info.first, dbc_->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(IamAuthPluginTest, Connect_Success_CacheExpire) {
    std::pair<std::string, bool> expired_token_info("expired_cached_token", true);
    std::pair<std::string, bool> valid_token_info("fresh_token", false);
    EXPECT_CALL(
        *mock_auth_provider_,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(expired_token_info))
        .WillOnce(testing::Return(valid_token_info));
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

    IamAuthPlugin plugin(dbc_, mock_base_plugin_, mock_auth_provider_, mock_dialect_, mock_odbc_helper_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(valid_token_info.first, dbc_->conn_attr.at(KEY_DB_PASSWORD));
}

TEST_F(IamAuthPluginTest, Connect_Fail_CacheMiss) {

    std::pair<std::string, bool> token_info("fresh_token", false);
    EXPECT_CALL(
        *mock_auth_provider_,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(token_info));
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_ERROR));

    IamAuthPlugin plugin(dbc_, mock_base_plugin_, mock_auth_provider_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}

TEST_F(IamAuthPluginTest, Connect_Fail_CacheHit_AccessError) {
    std::pair<std::string, bool> cached_token_info("cached_token", true);
    std::pair<std::string, bool> fresh_token_info("fresh_token", false);
    EXPECT_CALL(
        *mock_auth_provider_,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(cached_token_info))
        .WillOnce(testing::Return(fresh_token_info));
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillRepeatedly(testing::Return(SQL_ERROR));
    EXPECT_CALL(*mock_odbc_helper_, GetSqlStateAndLogMessage(testing::An<DBC*>(), testing::_))
        .WillOnce(testing::Return("28P01"));
    EXPECT_CALL(*mock_dialect_, IsSqlStateAccessError(testing::_, testing::An<const std::string&>()))
        .WillOnce(testing::Return(true));

    IamAuthPlugin plugin(dbc_, mock_base_plugin_, mock_auth_provider_, mock_dialect_, mock_odbc_helper_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}

TEST_F(IamAuthPluginTest, Connect_Fail_CacheHit_NonAccessError_NoRetry) {
    std::pair<std::string, bool> cached_token_info("cached_token", true);
    EXPECT_CALL(
        *mock_auth_provider_,
        GetToken(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(cached_token_info));
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_ERROR));
    EXPECT_CALL(*mock_odbc_helper_, GetSqlStateAndLogMessage(testing::An<DBC*>(), testing::_))
        .WillOnce(testing::Return("08001"));
    EXPECT_CALL(*mock_dialect_, IsSqlStateAccessError(testing::_, testing::An<const std::string&>()))
        .WillOnce(testing::Return(false));

    IamAuthPlugin plugin(dbc_, mock_base_plugin_, mock_auth_provider_, mock_dialect_, mock_odbc_helper_);
    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}
