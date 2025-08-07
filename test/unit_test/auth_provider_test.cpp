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

#include "mock_objects.h"

#include "../../driver/util/auth_provider.h"
#include "../../driver/util/aws_sdk_helper.h"
#include "../../driver/util/connection_string_keys.h"

class AuthProviderTest : public testing::Test {
protected:
    std::shared_ptr<MOCK_RDS_CLIENT> mock_rds_client;
    // Runs once per suite
    static void SetUpTestSuite() {
        AwsSdkHelper::Init();
    }
    static void TearDownTestSuite() {
        AwsSdkHelper::Shutdown();
    }

    void SetUp() override {
        mock_rds_client = std::make_shared<MOCK_RDS_CLIENT>();
        AuthProvider::ClearCache();
    }
    void TearDown() override {
        if (mock_rds_client) mock_rds_client.reset();
    }
};

TEST_F(AuthProviderTest, AuthTypeFromString_Test) {
    EXPECT_EQ(AuthType::ADFS, AuthProvider::AuthTypeFromString("adfs"));
    EXPECT_EQ(AuthType::ADFS, AuthProvider::AuthTypeFromString("ADFS"));
    EXPECT_EQ(AuthType::IAM, AuthProvider::AuthTypeFromString("iam"));
    EXPECT_EQ(AuthType::IAM, AuthProvider::AuthTypeFromString("IAM"));
    EXPECT_EQ(AuthType::OKTA, AuthProvider::AuthTypeFromString("okta"));
    EXPECT_EQ(AuthType::OKTA, AuthProvider::AuthTypeFromString("OKTA"));
    EXPECT_EQ(AuthType::DATABASE, AuthProvider::AuthTypeFromString("database"));
    EXPECT_EQ(AuthType::DATABASE, AuthProvider::AuthTypeFromString("DATABASE"));
    EXPECT_EQ(AuthType::INVALID, AuthProvider::AuthTypeFromString("garbage"));
    EXPECT_EQ(AuthType::INVALID, AuthProvider::AuthTypeFromString("unknown"));
}

TEST_F(AuthProviderTest, BuildCacheKey_Test) {
    std::string server = "host.com", region = "us-west-1", port = "1234", username = "my-user",
        expected = "host.com-us-west-1-1234-my-user";
    std::string built_cache_key = AuthProvider::BuildCacheKey(server, region, port, username);
    EXPECT_EQ(expected, built_cache_key);
}

TEST_F(AuthProviderTest, GetToken_Hit) {
    GTEST_SKIP() << "RDSClient::GenerateConnectAuthToken cannot be mocked, not virtual. Will need to refactor into a wrapped RDSClient";
    std::string server = "host.com", region = "us-west-1", port = "1234", username = "my-user",
        mock_token = "host.com:1234/?Action=Connect&DBUser=my-user";
    EXPECT_CALL(*mock_rds_client,
        GenerateConnectAuthToken(testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(mock_token));

    AuthProvider auth_provider(mock_rds_client);
    std::pair<std::string, bool> token_info = auth_provider.GetToken(server, region, port, username);
    EXPECT_FALSE(token_info.second);
    EXPECT_EQ(mock_token, token_info.first);
    token_info = auth_provider.GetToken(server, region, port, username);
    EXPECT_TRUE(token_info.second);
    EXPECT_EQ(mock_token, token_info.first);
}

TEST_F(AuthProviderTest, GetToken_Miss) {
    GTEST_SKIP() << "RDSClient::GenerateConnectAuthToken cannot be mocked, not virtual. Will need to refactor into a wrapped RDSClient";
    std::string server = "host.com", region = "us-west-1", port = "1234", username = "my-user",
        mock_token = "host.com:1234/?Action=Connect&DBUser=my-user";
    EXPECT_CALL(*mock_rds_client,
        GenerateConnectAuthToken(testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillRepeatedly(testing::Return(mock_token));

    AuthProvider auth_provider(mock_rds_client);
    std::pair<std::string, bool> token_info = auth_provider.GetToken(server, region, port, username);
    EXPECT_FALSE(token_info.second);
    EXPECT_EQ(mock_token, token_info.first);
}

TEST_F(AuthProviderTest, GetToken_Expired) {
    GTEST_SKIP() << "RDSClient::GenerateConnectAuthToken cannot be mocked, not virtual. Will need to refactor into a wrapped RDSClient";
    std::string server = "host.com", region = "us-west-1", port = "1234", username = "my-user",
        mock_token_first = "host.com:1234/?Action=Connect&DBUser=my-user&X-Amz-Date=123",
        mock_token_second = "host.com:1234/?Action=Connect&DBUser=my-user&X-Amz-Date=456";
    EXPECT_CALL(*mock_rds_client,
        GenerateConnectAuthToken(testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(mock_token_first))
        .WillOnce(testing::Return(mock_token_second));

    AuthProvider auth_provider(mock_rds_client);
    std::pair<std::string, bool> token_info_first = auth_provider.GetToken(server, region, port, username, true, false, std::chrono::milliseconds(-1));
    EXPECT_FALSE(token_info_first.second);
    EXPECT_EQ(mock_token_first, token_info_first.first);
    std::pair<std::string, bool> token_info_second = auth_provider.GetToken(server, region, port, username);
    EXPECT_FALSE(token_info_second.second);
    EXPECT_EQ(mock_token_second, token_info_second.first);

    EXPECT_NE(token_info_first.first, token_info_second.first);
}
