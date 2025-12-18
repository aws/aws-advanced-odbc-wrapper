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

#include "../../driver/util/connection_string_helper.h"
#include "../../driver/util/connection_string_keys.h"

class ConnectionStringHelperTest : public testing::Test {
protected:
    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConnectionStringHelperTest, ParseConnectionString) {
    std::string conn_str(
        std::string(KEY_DB_USERNAME) + "=jane_doe;" +
        std::string(KEY_DB_PASSWORD) + "=password;"
    );
    std::map<std::string, std::string> conn_map;

    ConnectionStringHelper::ParseConnectionString(conn_str, conn_map);

    EXPECT_EQ(2, conn_map.size());
    EXPECT_TRUE(conn_map.contains(KEY_DB_USERNAME));
    EXPECT_TRUE(conn_map.contains(KEY_DB_PASSWORD));
    EXPECT_STREQ("jane_doe", conn_map.at(KEY_DB_USERNAME).c_str());
    EXPECT_STREQ("password", conn_map.at(KEY_DB_PASSWORD).c_str());
}

TEST_F(ConnectionStringHelperTest, BuildFullConnectionString) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DB_USERNAME, "jane_doe");
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "password");
    conn_map.insert_or_assign(KEY_AUTH_TYPE, "iam");
    std::string expected(
        std::string(KEY_DB_USERNAME) + "=jane_doe;" +
        std::string(KEY_DB_PASSWORD) + "=password;" +
        std::string(KEY_AUTH_TYPE) + "=iam;"
    );

    std::map<std::string, std::string> result_map;
    std::string conn_str = ConnectionStringHelper::BuildFullConnectionString(conn_map);
    ConnectionStringHelper::ParseConnectionString(conn_str, result_map);

    EXPECT_EQ(3, result_map.size());
    EXPECT_TRUE(result_map.contains(KEY_DB_USERNAME));
    EXPECT_TRUE(result_map.contains(KEY_DB_PASSWORD));
    EXPECT_TRUE(result_map.contains(KEY_AUTH_TYPE));
    EXPECT_STREQ("jane_doe", result_map.at(KEY_DB_USERNAME).c_str());
    EXPECT_STREQ("password", result_map.at(KEY_DB_PASSWORD).c_str());
    EXPECT_STREQ("iam", result_map.at(KEY_AUTH_TYPE).c_str());
}

TEST_F(ConnectionStringHelperTest, BuildMinimumConnectionString) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DB_USERNAME, "jane_doe");
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "password");
    conn_map.insert_or_assign(KEY_AUTH_TYPE, "iam");

    std::map<std::string, std::string> result_map;
    std::string conn_str = ConnectionStringHelper::BuildMinimumConnectionString(conn_map);
    ConnectionStringHelper::ParseConnectionString(conn_str, result_map);

    EXPECT_EQ(2, result_map.size());
    EXPECT_TRUE(result_map.contains(KEY_DB_USERNAME));
    EXPECT_TRUE(result_map.contains(KEY_DB_PASSWORD));
    EXPECT_STREQ("jane_doe", result_map.at(KEY_DB_USERNAME).c_str());
    EXPECT_STREQ("password", result_map.at(KEY_DB_PASSWORD).c_str());
}

TEST_F(ConnectionStringHelperTest, MaskConnectionString) {
    std::string conn_str(
        std::string(KEY_DB_USERNAME) + "=jane_doe;" +
        std::string(KEY_DB_PASSWORD) + "=password;" +
        std::string(KEY_IDP_PASSWORD) + "=idp_password;"
    );

    std::string expected(
        std::string(KEY_DB_USERNAME) + "=jane_doe;" +
        std::string(KEY_DB_PASSWORD) + "=[REDACTED];" +
        std::string(KEY_IDP_PASSWORD) + "=[REDACTED];"
    );

    std::string masked_str = ConnectionStringHelper::MaskSensitiveInformation(conn_str);

    EXPECT_EQ(expected, masked_str);
}

TEST_F(ConnectionStringHelperTest, GetRealKeyName) {
    // UID
    EXPECT_EQ(std::string(KEY_DB_USERNAME), ConnectionStringHelper::GetRealKeyName(ALIAS_KEY_USERNAME_1));
    EXPECT_EQ(std::string(KEY_DB_USERNAME), ConnectionStringHelper::GetRealKeyName(ALIAS_KEY_USERNAME_2));
    // PWD
    EXPECT_EQ(std::string(KEY_DB_PASSWORD), ConnectionStringHelper::GetRealKeyName(ALIAS_KEY_PASSWORD_1));
    EXPECT_EQ(std::string(KEY_DB_PASSWORD), ConnectionStringHelper::GetRealKeyName(ALIAS_KEY_PASSWORD_2));
    EXPECT_EQ(std::string(KEY_DB_PASSWORD), ConnectionStringHelper::GetRealKeyName(ALIAS_KEY_PASSWORD_3));
    // Server
    EXPECT_EQ(std::string(KEY_SERVER), ConnectionStringHelper::GetRealKeyName(ALIAS_KEY_SERVER_1));
    // Default, no aliases
    EXPECT_EQ(std::string(KEY_AUTH_TYPE), ConnectionStringHelper::GetRealKeyName(KEY_AUTH_TYPE));
}
