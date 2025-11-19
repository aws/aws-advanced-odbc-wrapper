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

#include <map>

#include "../../driver/util/attribute_validator.h"
#include "../../driver/util/connection_string_keys.h"

class AttributeValidatorTest : public testing::Test {
protected:
    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(AttributeValidatorTest, ShouldKeyBeUnsignedInt) {
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_TOKEN_EXPIRATION));
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_IAM_PORT));
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_IDP_PORT));
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_HTTP_SOCKET_TIMEOUT));
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_HTTP_CONNECT_TIMEOUT));
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_IGNORE_TOPOLOGY_REQUEST));
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_HIGH_REFRESH_RATE));
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_REFRESH_RATE));
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_FAILOVER_TIMEOUT));
    EXPECT_TRUE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_LIMITLESS_MONITOR_INTERVAL_MS));

    EXPECT_FALSE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_DATABASE));
    EXPECT_FALSE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_SERVER));
    EXPECT_FALSE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_DRIVER));
    EXPECT_FALSE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_AUTH_TYPE));
    EXPECT_FALSE(AttributeValidator::ShouldKeyBeUnsignedInt(KEY_REGION));
}

TEST_F(AttributeValidatorTest, IsValueInt) {
    EXPECT_TRUE(AttributeValidator::IsValueUnsignedInt("1234"));
    EXPECT_FALSE(AttributeValidator::IsValueUnsignedInt("-1234"));

    EXPECT_FALSE(AttributeValidator::IsValueUnsignedInt("99999999999999999999"));

    EXPECT_FALSE(AttributeValidator::IsValueUnsignedInt("abc"));
    EXPECT_FALSE(AttributeValidator::IsValueUnsignedInt("abc123efg"));
    EXPECT_FALSE(AttributeValidator::IsValueUnsignedInt("1e2"));
    EXPECT_FALSE(AttributeValidator::IsValueUnsignedInt("123efg"));
}

TEST_F(AttributeValidatorTest, ValidateMap) {
    std::unordered_set<std::string> invalid_params_set;

    std::map<std::string, std::string> valid_map = {
        {KEY_PORT, "1234"},
        {KEY_REFRESH_RATE, "5678"},
        {KEY_SERVER, "my-db-host"}
    };
    invalid_params_set = AttributeValidator::ValidateMap(valid_map);
    EXPECT_TRUE(invalid_params_set.empty());

    std::map<std::string, std::string> invalid_map = {
        {KEY_PORT, "5 hundred"},
        {KEY_REFRESH_RATE, "million"},
        {KEY_FAILOVER_TIMEOUT, "1234"},
        {KEY_SERVER, "my-db-host"}
    };
    std::unordered_set<std::string> expected_invalid_params = {
        KEY_PORT, KEY_REFRESH_RATE
    };
    invalid_params_set = AttributeValidator::ValidateMap(invalid_map);
    for (std::string expected : expected_invalid_params) {
        EXPECT_TRUE(invalid_params_set.contains(expected));
    }
}
