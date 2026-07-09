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

#include <string>

#include "auth_mock_objects.h"

#include "../../driver/util/auth_provider.h"
#include "../../driver/util/aws_sdk_helper.h"

// AWS_PROFILE_TEST_RESOURCES_DIR is defined by CMake and points at the
// test/unit_test/resources directory, which holds the committed aws_config
#ifndef AWS_PROFILE_TEST_RESOURCES_DIR
    #error "AWS_PROFILE_TEST_RESOURCES_DIR must be defined by the build"
#endif

namespace {
    // Set environment var in a cross-platform way to point the SDK at the test config.
    void SetEnvVar(const char* name, const std::string& value) {
#ifdef _WIN32
        _putenv_s(name, value.c_str());
#else
        setenv(name, value.c_str(), 1);
#endif
    }

    const std::string test_resource_dir = AWS_PROFILE_TEST_RESOURCES_DIR;
    const std::string test_config_file = test_resource_dir + "/aws_config";
}

class AuthProviderTest : public testing::Test {
protected:
    std::shared_ptr<MOCK_RDS_CLIENT> mock_rds_client;
    // Runs once per suite
    static void SetUpTestSuite() {
        AwsSdkHelper::EnsureInitialized();
    }

    void SetUp() override {
        mock_rds_client = std::make_shared<MOCK_RDS_CLIENT>();
        AuthProvider::ClearCache();
        SetEnvVar("AWS_CONFIG_FILE", test_config_file);
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

TEST_F(AuthProviderTest, GetRegionForProfile_EmptyProfile_ReturnsEmpty) {
    EXPECT_EQ("", AuthProvider::GetRegionForProfile(""));
}

TEST_F(AuthProviderTest, GetRegionForProfile_Resolved) {
    EXPECT_EQ("us-west-2", AuthProvider::GetRegionForProfile("dev"));
}

TEST_F(AuthProviderTest, GetRegionForProfile_NoRegion_ReturnsEmpty) {
    EXPECT_EQ("", AuthProvider::GetRegionForProfile("no_region"));
}

TEST_F(AuthProviderTest, GetRegionForProfile_NonexistentProfile_ReturnsEmpty) {
    EXPECT_EQ("", AuthProvider::GetRegionForProfile("does_not_exist"));
}

TEST_F(AuthProviderTest, BuildCacheKey_Test) {
    std::string server = "host.com", region = "us-west-1", port = "1234", username = "my-user",
        expected = "host.com-us-west-1-1234-my-user";
    std::string built_cache_key = AuthProvider::BuildCacheKey(server, region, port, username);
    EXPECT_EQ(expected, built_cache_key);
}

TEST_F(AuthProviderTest, GetToken_CacheMiss) {
    std::string server = "host.com", region = "us-west-1", port = "1234", username = "my-user";

    EXPECT_CALL(*mock_rds_client, GenerateConnectAuthToken(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::Return("token"));

    AuthProvider auth_provider(mock_rds_client);
    std::pair<std::string, bool> token_info = auth_provider.GetToken(server, region, port, username);
    EXPECT_EQ("token", token_info.first);
    EXPECT_FALSE(token_info.second);
}

TEST_F(AuthProviderTest, GetToken_CacheHit) {
    std::string server = "host.com", region = "us-west-1", port = "1234", username = "my-user";

    EXPECT_CALL(*mock_rds_client, GenerateConnectAuthToken(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::Return("token"));

    AuthProvider auth_provider(mock_rds_client);
    std::pair<std::string, bool> token_info = auth_provider.GetToken(server, region, port, username);
    EXPECT_FALSE(token_info.second);
    std::pair<std::string, bool> token_info_cached = auth_provider.GetToken(server, region, port, username);
    EXPECT_TRUE(token_info_cached.second);
    EXPECT_EQ(token_info_cached.first, token_info.first);
}

TEST_F(AuthProviderTest, GetToken_Expired) {
    std::string server = "host.com", region = "us-west-1", port = "1234", username = "my-user";

    EXPECT_CALL(*mock_rds_client, GenerateConnectAuthToken(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::Return("token-1"))
        .WillOnce(testing::Return("token-2"));

    AuthProvider auth_provider(mock_rds_client);
    std::pair<std::string, bool> token_info_first = auth_provider.GetToken(server, region, port, username, true, false, std::chrono::milliseconds(-99999));
    EXPECT_FALSE(token_info_first.second);
    std::pair<std::string, bool> token_info_second = auth_provider.GetToken(server, region, port, username);
    EXPECT_FALSE(token_info_second.second);

    EXPECT_NE(token_info_first.first, token_info_second.first);
}
