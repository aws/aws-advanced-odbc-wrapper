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
    RDS_STR conn_str(
        KEY_DB_USERNAME TEXT("=jane_doe;")
        KEY_DB_PASSWORD TEXT("=password;")
    );
    std::map<RDS_STR, RDS_STR> conn_map;

    ConnectionStringHelper::ParseConnectionString(conn_str, conn_map);

    EXPECT_EQ(2, conn_map.size());
    EXPECT_TRUE(conn_map.contains(KEY_DB_USERNAME));
    EXPECT_TRUE(conn_map.contains(KEY_DB_PASSWORD));
    EXPECT_STREQ(TEXT("jane_doe"), conn_map.at(KEY_DB_USERNAME).c_str());
    EXPECT_STREQ(TEXT("password"), conn_map.at(KEY_DB_PASSWORD).c_str());
}

TEST_F(ConnectionStringHelperTest, BuildFullConnectionString) {
    std::map<RDS_STR, RDS_STR> conn_map;
    conn_map.insert_or_assign(KEY_DB_USERNAME, "jane_doe");
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "password");
    conn_map.insert_or_assign(KEY_AUTH_TYPE, "iam");
    RDS_STR expected(
        KEY_DB_USERNAME TEXT("=jane_doe;")
        KEY_DB_PASSWORD TEXT("=password;")
        KEY_AUTH_TYPE TEXT("=iam;")
    );

    std::map<RDS_STR, RDS_STR> result_map;
    RDS_STR conn_str = ConnectionStringHelper::BuildFullConnectionString(conn_map);
    ConnectionStringHelper::ParseConnectionString(conn_str, result_map);

    EXPECT_EQ(3, result_map.size());
    EXPECT_TRUE(result_map.contains(KEY_DB_USERNAME));
    EXPECT_TRUE(result_map.contains(KEY_DB_PASSWORD));
    EXPECT_TRUE(result_map.contains(KEY_AUTH_TYPE));
    EXPECT_STREQ(TEXT("jane_doe"), result_map.at(KEY_DB_USERNAME).c_str());
    EXPECT_STREQ(TEXT("password"), result_map.at(KEY_DB_PASSWORD).c_str());
    EXPECT_STREQ(TEXT("iam"), result_map.at(KEY_AUTH_TYPE).c_str());
}

TEST_F(ConnectionStringHelperTest, BuildMinimumConnectionString) {
    std::map<RDS_STR, RDS_STR> conn_map;
    conn_map.insert_or_assign(KEY_DB_USERNAME, "jane_doe");
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "password");
    conn_map.insert_or_assign(KEY_AUTH_TYPE, "iam");

    std::map<RDS_STR, RDS_STR> result_map;
    RDS_STR conn_str = ConnectionStringHelper::BuildMinimumConnectionString(conn_map);
    ConnectionStringHelper::ParseConnectionString(conn_str, result_map);

    EXPECT_EQ(2, result_map.size());
    EXPECT_TRUE(result_map.contains(KEY_DB_USERNAME));
    EXPECT_TRUE(result_map.contains(KEY_DB_PASSWORD));
    EXPECT_STREQ(TEXT("jane_doe"), result_map.at(KEY_DB_USERNAME).c_str());
    EXPECT_STREQ(TEXT("password"), result_map.at(KEY_DB_PASSWORD).c_str());
}

TEST_F(ConnectionStringHelperTest, MaskConnectionString) {
    RDS_STR conn_str(
        KEY_DB_USERNAME TEXT("=jane_doe;")
        KEY_DB_PASSWORD TEXT("=password;")
        KEY_IDP_PASSWORD TEXT("=idp_password;")
    );

    RDS_STR expected(
        KEY_DB_USERNAME TEXT("=jane_doe;")
        KEY_DB_PASSWORD TEXT("=[REDACTED];")
        KEY_IDP_PASSWORD TEXT("=[REDACTED];")
    );

    RDS_STR masked_str = ConnectionStringHelper::MaskSensitiveInformation(conn_str);

    EXPECT_STREQ(expected.c_str(), masked_str.c_str());
}
