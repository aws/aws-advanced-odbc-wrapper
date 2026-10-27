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

TEST_F(ConnectionStringHelperTest, ParseConnectionStringQuotedValue) {
    const std::string conn_str(
        std::string(KEY_DRIVER) + "={AWS Advanced ODBC Wrapper};" +
        std::string(KEY_DB_PASSWORD) + "={pa;ss=wo{rd};" +
        std::string(KEY_DB_USERNAME) + "=jane_doe"
    );
    std::map<std::string, std::string> conn_map;

    ConnectionStringHelper::ParseConnectionString(conn_str, conn_map);

    EXPECT_EQ(3, conn_map.size());
    EXPECT_EQ("AWS Advanced ODBC Wrapper", conn_map.at(KEY_DRIVER));
    EXPECT_EQ("pa;ss=wo{rd", conn_map.at(KEY_DB_PASSWORD));
    EXPECT_EQ("jane_doe", conn_map.at(KEY_DB_USERNAME));
}

TEST_F(ConnectionStringHelperTest, ParseConnectionStringEscapedClosingBrace) {
    const std::string conn_str(
        std::string(KEY_DB_PASSWORD) + "={pa}}ss};" +
        std::string(KEY_DB_USERNAME) + "=jane_doe;"
    );
    std::map<std::string, std::string> conn_map;

    ConnectionStringHelper::ParseConnectionString(conn_str, conn_map);

    EXPECT_EQ(2, conn_map.size());
    EXPECT_EQ("pa}ss", conn_map.at(KEY_DB_PASSWORD));
    EXPECT_EQ("jane_doe", conn_map.at(KEY_DB_USERNAME));
}

TEST_F(ConnectionStringHelperTest, ParseConnectionStringUnquotedValueKeepsSpecialCharacters) {
    const std::string conn_str(
        std::string(KEY_DB_PASSWORD) + "=pa=ss}word;" +
        std::string(KEY_DB_USERNAME) + "=jane_doe"
    );
    std::map<std::string, std::string> conn_map;

    ConnectionStringHelper::ParseConnectionString(conn_str, conn_map);

    EXPECT_EQ(2, conn_map.size());
    EXPECT_EQ("pa=ss}word", conn_map.at(KEY_DB_PASSWORD));
    EXPECT_EQ("jane_doe", conn_map.at(KEY_DB_USERNAME));
}

TEST_F(ConnectionStringHelperTest, ParseConnectionStringUnterminatedQuoteKeepsFollowingAttributes) {
    const std::string conn_str(
        std::string(KEY_DB_PASSWORD) + "={password;" +
        std::string(KEY_DB_USERNAME) + "=jane_doe"
    );
    std::map<std::string, std::string> conn_map;

    ConnectionStringHelper::ParseConnectionString(conn_str, conn_map);

    EXPECT_EQ(2, conn_map.size());
    EXPECT_EQ("{password", conn_map.at(KEY_DB_PASSWORD));
    EXPECT_EQ("jane_doe", conn_map.at(KEY_DB_USERNAME));
}

TEST_F(ConnectionStringHelperTest, ParseConnectionStringMalformedAttributes) {
    const std::string conn_str(
        ";;no_value; " + std::string(KEY_DB_USERNAME) + " =jane_doe;" +
        std::string(KEY_DB_PASSWORD) + "=;trailing"
    );
    std::map<std::string, std::string> conn_map;

    ConnectionStringHelper::ParseConnectionString(conn_str, conn_map);

    EXPECT_EQ(1, conn_map.size());
    EXPECT_EQ("jane_doe", conn_map.at(KEY_DB_USERNAME));
}

TEST_F(ConnectionStringHelperTest, ConnectionStringRoundTripQuotesSpecialValues) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DB_USERNAME, "jane_doe");
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "pa;ss}wo{rd");
    conn_map.insert_or_assign(KEY_SERVER, "{not-a-quoted-value}");

    const std::string conn_str = ConnectionStringHelper::BuildFullConnectionString(conn_map);
    EXPECT_NE(std::string::npos, conn_str.find(std::string(KEY_DB_PASSWORD) + "={pa;ss}}wo{rd}"));

    std::map<std::string, std::string> result_map;
    ConnectionStringHelper::ParseConnectionString(conn_str, result_map);

    EXPECT_EQ(conn_map, result_map);
}

TEST_F(ConnectionStringHelperTest, MinimumConnectionStringRoundTripQuotesSpecialValues) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "pa;ss");
    conn_map.insert_or_assign(KEY_AUTH_TYPE, VALUE_AUTH_IAM);

    std::map<std::string, std::string> result_map;
    const std::string conn_str = ConnectionStringHelper::BuildMinimumConnectionString(conn_map);
    ConnectionStringHelper::ParseConnectionString(conn_str, result_map);

    EXPECT_EQ(1, result_map.size());
    EXPECT_EQ("pa;ss", result_map.at(KEY_DB_PASSWORD));
}

TEST_F(ConnectionStringHelperTest, GeneratedPasswordIsAlwaysQuotedForUnderlyingDriver) {
    const std::string token("host:5432/?Action=connect&X-Amz-Credential=AKIA%2F20261027%2Frds-db");
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_SERVER, "host");
    ConnectionStringHelper::SetGeneratedPassword(conn_map, token);

    const std::string conn_str = ConnectionStringHelper::BuildMinimumConnectionString(conn_map);

    EXPECT_NE(std::string::npos, conn_str.find(std::string(KEY_DB_PASSWORD) + "={" + token + "}"));
    // The marker is internal and must not reach the underlying driver
    EXPECT_EQ(std::string::npos, conn_str.find(KEY_LITERAL_CREDENTIALS));

    std::map<std::string, std::string> result_map;
    ConnectionStringHelper::ParseConnectionString(conn_str, result_map);
    EXPECT_EQ(token, result_map.at(KEY_DB_PASSWORD));
}

TEST_F(ConnectionStringHelperTest, PasswordFromBaseDsnIsNotQuoted) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "pa%2Bss");

    EXPECT_EQ(std::string(KEY_DB_PASSWORD) + "=pa%2Bss",
        ConnectionStringHelper::BuildMinimumConnectionString(conn_map));
}

TEST_F(ConnectionStringHelperTest, GeneratedPasswordMarkerSurvivesRoundTrip) {
    std::map<std::string, std::string> conn_map;
    ConnectionStringHelper::SetGeneratedPassword(conn_map, "pa;ss}word");

    std::map<std::string, std::string> result_map;
    const std::string full = ConnectionStringHelper::BuildFullConnectionString(conn_map);
    ConnectionStringHelper::ParseConnectionString(full, result_map);

    EXPECT_EQ(conn_map, result_map);
    EXPECT_EQ(std::string(KEY_DB_PASSWORD) + "={pa;ss}}word}",
        ConnectionStringHelper::BuildMinimumConnectionString(result_map));
    // The marker must not be returned to the application either
    EXPECT_EQ(std::string::npos,
        ConnectionStringHelper::RemoveInternalWrapperKeys(full).find(KEY_LITERAL_CREDENTIALS));
}

TEST_F(ConnectionStringHelperTest, MaskConnectionStringQuotedValue) {
    const std::string conn_str(
        std::string(KEY_DB_PASSWORD) + "={pa;ss}}word};" +
        std::string(KEY_DB_USERNAME) + "=jane_doe"
    );

    const std::string masked_str = ConnectionStringHelper::MaskSensitiveInformation(conn_str);

    EXPECT_EQ(std::string(KEY_DB_PASSWORD) + "=[REDACTED];" + std::string(KEY_DB_USERNAME) + "=jane_doe", masked_str);
}

TEST_F(ConnectionStringHelperTest, RemoveInternalWrapperKeys) {
    const std::string conn_str(
        std::string(KEY_DB_USERNAME) + "=jane_doe;" +
        std::string(KEY_RDS_TEST_CONN) + "={1;1};" +
        std::string(KEY_SRW_SKIP) + "=1;" +
        std::string(KEY_AUTH_TYPE) + "=" + VALUE_AUTH_IAM
    );

    const std::string result = ConnectionStringHelper::RemoveInternalWrapperKeys(conn_str);

    EXPECT_EQ(std::string(KEY_DB_USERNAME) + "=jane_doe;" + std::string(KEY_AUTH_TYPE) + "=" + VALUE_AUTH_IAM, result);
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

TEST_F(ConnectionStringHelperTest, DsnOnlyOutputHidesCredentials) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DSN, "my-dsn");
    conn_map.insert_or_assign(KEY_DB_USERNAME, "jane_doe");
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "secret_password");
    conn_map.insert_or_assign(KEY_DSN_ONLY_OUTPUT, VALUE_BOOL_TRUE);

    const std::string full = ConnectionStringHelper::BuildFullConnectionString(conn_map);
    const std::string result = ConnectionStringHelper::BuildDsnOnlyConnectionString(conn_map, full);

    EXPECT_EQ(std::string(KEY_DSN) + "=my-dsn", result);
    EXPECT_EQ(std::string::npos, result.find("jane_doe"));
    EXPECT_EQ(std::string::npos, result.find("secret_password"));
}

TEST_F(ConnectionStringHelperTest, DsnOnlyOutputDisabledReturnsFull) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DSN, "my-dsn");
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "secret_password");

    const std::string full = ConnectionStringHelper::BuildFullConnectionString(conn_map);
    EXPECT_EQ(full, ConnectionStringHelper::BuildDsnOnlyConnectionString(conn_map, full));
}

TEST_F(ConnectionStringHelperTest, DsnOnlyOutputWithoutDsnReturnsEmpty) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "secret_password");
    conn_map.insert_or_assign(KEY_DSN_ONLY_OUTPUT, VALUE_BOOL_TRUE);

    const std::string full = ConnectionStringHelper::BuildFullConnectionString(conn_map);
    const std::string expected = "DSN=[empty]";
    EXPECT_EQ(expected, ConnectionStringHelper::BuildDsnOnlyConnectionString(conn_map, full));
}

TEST_F(ConnectionStringHelperTest, GetDeprecatedKeysInUse) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DB_USERNAME, "jane_doe");

    // Absent
    EXPECT_TRUE(ConnectionStringHelper::GetDeprecatedKeysInUse(conn_map).empty());

    // Present but disabled, a DSN carries every attribute the setup dialog knows about
    conn_map.insert_or_assign(KEY_EXTRA_URL_ENCODE, VALUE_BOOL_FALSE);
    EXPECT_TRUE(ConnectionStringHelper::GetDeprecatedKeysInUse(conn_map).empty());

    conn_map.insert_or_assign(KEY_EXTRA_URL_ENCODE, VALUE_BOOL_TRUE);
    EXPECT_EQ(std::vector<std::string>{KEY_EXTRA_URL_ENCODE}, ConnectionStringHelper::GetDeprecatedKeysInUse(conn_map));
}

TEST_F(ConnectionStringHelperTest, IsAwsOdbcKey) {
    EXPECT_TRUE(ConnectionStringHelper::IsAwsOdbcKey(KEY_SSO_ALLOW_INTERACTIVE));
    EXPECT_TRUE(ConnectionStringHelper::IsAwsOdbcKey(KEY_SSO_START_URL));
    EXPECT_TRUE(ConnectionStringHelper::IsAwsOdbcKey(KEY_AUTH_TYPE));
    EXPECT_FALSE(ConnectionStringHelper::IsAwsOdbcKey(KEY_DB_USERNAME));
    EXPECT_FALSE(ConnectionStringHelper::IsAwsOdbcKey(KEY_SERVER));
}

TEST_F(ConnectionStringHelperTest, MinimumConnectionStringDropsSsoAllowInteractive) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DB_USERNAME, "jane_doe");
    conn_map.insert_or_assign(KEY_SSO_ALLOW_INTERACTIVE, VALUE_BOOL_TRUE);

    std::map<std::string, std::string> result_map;
    const std::string conn_str = ConnectionStringHelper::BuildMinimumConnectionString(conn_map);
    ConnectionStringHelper::ParseConnectionString(conn_str, result_map);

    EXPECT_TRUE(result_map.contains(KEY_DB_USERNAME));
    EXPECT_FALSE(result_map.contains(KEY_SSO_ALLOW_INTERACTIVE));
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
