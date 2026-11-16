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

    EXPECT_TRUE(ConnectionStringHelper::ParseConnectionString(conn_str, conn_map));

    // The three attributes and the marker of the values that were enclosed in braces
    EXPECT_EQ(4, conn_map.size());
    EXPECT_EQ("AWS Advanced ODBC Wrapper", conn_map.at(KEY_DRIVER));
    EXPECT_EQ("pa;ss=wo{rd", conn_map.at(KEY_DB_PASSWORD));
    EXPECT_EQ("jane_doe", conn_map.at(KEY_DB_USERNAME));
    EXPECT_TRUE(ConnectionStringHelper::IsLiteralKey(conn_map, KEY_DRIVER));
    EXPECT_TRUE(ConnectionStringHelper::IsLiteralKey(conn_map, KEY_DB_PASSWORD));
    EXPECT_FALSE(ConnectionStringHelper::IsLiteralKey(conn_map, KEY_DB_USERNAME));
}

TEST_F(ConnectionStringHelperTest, ParseConnectionStringEscapedClosingBrace) {
    const std::string conn_str(
        std::string(KEY_DB_PASSWORD) + "={pa}}ss};" +
        std::string(KEY_DB_USERNAME) + "=jane_doe;"
    );
    std::map<std::string, std::string> conn_map;

    EXPECT_TRUE(ConnectionStringHelper::ParseConnectionString(conn_str, conn_map));

    EXPECT_EQ(3, conn_map.size());
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

    // Reported as malformed, still parsed so the following attributes are kept
    EXPECT_FALSE(ConnectionStringHelper::ParseConnectionString(conn_str, conn_map));

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

    // The values that had to be enclosed in braces stay enclosed when the string is built again
    EXPECT_TRUE(ConnectionStringHelper::IsLiteralKey(result_map, KEY_DB_PASSWORD));
    EXPECT_TRUE(ConnectionStringHelper::IsLiteralKey(result_map, KEY_SERVER));
    result_map.erase(KEY_LITERAL_KEYS);
    EXPECT_EQ(conn_map, result_map);
}

TEST_F(ConnectionStringHelperTest, MinimumConnectionStringRoundTripQuotesSpecialValues) {
    std::map<std::string, std::string> conn_map;
    conn_map.insert_or_assign(KEY_DB_PASSWORD, "pa;ss");
    conn_map.insert_or_assign(KEY_AUTH_TYPE, VALUE_AUTH_IAM);

    std::map<std::string, std::string> result_map;
    const std::string conn_str = ConnectionStringHelper::BuildMinimumConnectionString(conn_map);
    ConnectionStringHelper::ParseConnectionString(conn_str, result_map);

    EXPECT_FALSE(result_map.contains(KEY_AUTH_TYPE));
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
    EXPECT_EQ(std::string::npos, conn_str.find(KEY_LITERAL_KEYS));

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
        ConnectionStringHelper::RemoveInternalWrapperKeys(full).find(KEY_LITERAL_KEYS));
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

    // Values other than 1 never enabled the option
    for (const std::string value : {"true", "false", "no", ""}) {
        conn_map.insert_or_assign(KEY_EXTRA_URL_ENCODE, value);
        EXPECT_TRUE(ConnectionStringHelper::GetDeprecatedKeysInUse(conn_map).empty()) << value;
    }

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

// A password the application encloses in braces reaches the underlying driver still enclosed. psqlODBC takes
// a braced PWD literally and URL-decodes an unbraced one ('+' becomes a space, '%41' becomes 'A').
TEST_F(ConnectionStringHelperTest, UserQuotedPasswordStaysQuotedForUnderlyingDriver) {
    std::map<std::string, std::string> conn_map;
    ConnectionStringHelper::ParseConnectionString("PWD={Odbc+Pass%41word}", conn_map);

    EXPECT_EQ("Odbc+Pass%41word", conn_map.at(KEY_DB_PASSWORD));
    EXPECT_EQ(std::string(KEY_DB_PASSWORD) + "={Odbc+Pass%41word}",
        ConnectionStringHelper::BuildMinimumConnectionString(conn_map));
}

// A password the application does not enclose in braces reaches the underlying driver unenclosed,
// so psqlODBC decodes a password the application URL-encoded for it.
TEST_F(ConnectionStringHelperTest, UserUnquotedPasswordStaysUnquotedForUnderlyingDriver) {
    std::map<std::string, std::string> conn_map;
    ConnectionStringHelper::ParseConnectionString("PWD=Odbc%2BPass%2541word", conn_map);

    EXPECT_EQ(std::string(KEY_DB_PASSWORD) + "=Odbc%2BPass%2541word",
        ConnectionStringHelper::BuildMinimumConnectionString(conn_map));
}

// Masking a long braced value, such as an IAM token carrying a session token, does not throw. MSVC STL
// before 14.51 throws regex_error(error_stack) when a regex repeats a group once per character.
TEST_F(ConnectionStringHelperTest, MaskConnectionStringLongQuotedValue) {
    for (const size_t length : {size_t{1452}, size_t{4096}, size_t{16384}}) {
        const std::string conn_str(
            std::string(KEY_DB_PASSWORD) + "={" + std::string(length, 'a') + "};" +
            std::string(KEY_DB_USERNAME) + "=jane_doe"
        );

        std::string masked_str;
        EXPECT_NO_THROW(masked_str = ConnectionStringHelper::MaskSensitiveInformation(conn_str)) << "length " << length;
        EXPECT_EQ(std::string(KEY_DB_PASSWORD) + "=[REDACTED];" + std::string(KEY_DB_USERNAME) + "=jane_doe", masked_str)
            << "length " << length;
    }
}

// Values the application writes reach the underlying driver exactly as written, braces included,
// so the driver applies its own rules. psqlODBC URL-decodes an unbraced PWD, ConnSettings and pqopt,
// MySQL Connector/ODBC 8.0.30+ unescapes "}}" even outside braces.
TEST_F(ConnectionStringHelperTest, UserValuesPassThroughAsWritten) {
    const std::string conn_str(
        "CONNSETTINGS={SET application_name TO 'a+b'};"
        "PWD={a}}}}b+c};"
        "SSLMODE=require;"
        "UID=a}}b%41"
    );
    std::map<std::string, std::string> conn_map;
    EXPECT_TRUE(ConnectionStringHelper::ParseConnectionString(conn_str, conn_map));

    EXPECT_EQ("a}}b+c", conn_map.at(KEY_DB_PASSWORD));
    EXPECT_EQ(conn_str, ConnectionStringHelper::BuildMinimumConnectionString(conn_map));
}

// A later connection string decides again whether a value is braced, e.g. on a reused connection handle
TEST_F(ConnectionStringHelperTest, LaterUnquotedValueIsNotQuoted) {
    std::map<std::string, std::string> conn_map;
    ConnectionStringHelper::ParseConnectionString("PWD={a+b}", conn_map);
    ConnectionStringHelper::ParseConnectionString("PWD=c%2Bd", conn_map);

    EXPECT_EQ("PWD=c%2Bd", ConnectionStringHelper::BuildMinimumConnectionString(conn_map));
}

// A SQLConnect password set after a braced or unbraced connection string value is always braced
TEST_F(ConnectionStringHelperTest, GeneratedPasswordReplacesUnquotedPassword) {
    std::map<std::string, std::string> conn_map;
    ConnectionStringHelper::ParseConnectionString("PWD=a%2Bb", conn_map);
    ConnectionStringHelper::SetGeneratedPassword(conn_map, "x+y}z");

    EXPECT_EQ("PWD={x+y}}z}", ConnectionStringHelper::BuildMinimumConnectionString(conn_map));
}

// An explicitly empty value is passed on
TEST_F(ConnectionStringHelperTest, QuotedEmptyValueIsKept) {
    std::map<std::string, std::string> conn_map;
    EXPECT_TRUE(ConnectionStringHelper::ParseConnectionString("PWD={};UID=jane_doe", conn_map));

    EXPECT_EQ("PWD={};UID=jane_doe", ConnectionStringHelper::BuildMinimumConnectionString(conn_map));
}

TEST_F(ConnectionStringHelperTest, ParseConnectionStringReportsMalformedQuotedValue) {
    std::map<std::string, std::string> conn_map;
    // Text after the closing brace
    EXPECT_FALSE(ConnectionStringHelper::ParseConnectionString("PWD={abc}def;UID=jane_doe", conn_map));
    // Literal closing brace not doubled
    EXPECT_FALSE(ConnectionStringHelper::ParseConnectionString("PWD={a}b};UID=jane_doe", conn_map));
    // Not closed
    EXPECT_FALSE(ConnectionStringHelper::ParseConnectionString("UID=jane_doe;PWD={abc", conn_map));
    // Whitespace after the closing brace is accepted
    EXPECT_TRUE(ConnectionStringHelper::ParseConnectionString("PWD={abc} ;UID=jane_doe", conn_map));
    EXPECT_EQ("abc", conn_map.at(KEY_DB_PASSWORD));
}

// Monitoring threads rebuild a connection string from the attributes and parse it again
TEST_F(ConnectionStringHelperTest, QuotedValuesSurviveFullConnectionStringRoundTrip) {
    std::map<std::string, std::string> conn_map;
    ConnectionStringHelper::ParseConnectionString("PWD={a+b};SERVER=host;UID=jane_doe", conn_map);

    const std::string full = ConnectionStringHelper::BuildFullConnectionString(conn_map);
    std::map<std::string, std::string> result_map;
    EXPECT_TRUE(ConnectionStringHelper::ParseConnectionString(full, result_map));

    EXPECT_EQ(conn_map, result_map);
    EXPECT_EQ("PWD={a+b};SERVER=host;UID=jane_doe", ConnectionStringHelper::BuildMinimumConnectionString(result_map));
    // The output connection string returned to the application keeps the braces, not the marker
    EXPECT_EQ("PWD={a+b};SERVER=host;UID=jane_doe", ConnectionStringHelper::RemoveInternalWrapperKeys(full));
}

TEST_F(ConnectionStringHelperTest, BaseConnectionStringKeepsBraces) {
    std::map<std::string, std::string> base_conn_map;
    ConnectionStringHelper::ParseConnectionString("PWD={a+b};UID=base_user;SSLMODE=require", base_conn_map);
    std::map<std::string, std::string> conn_map;
    ConnectionStringHelper::ParseConnectionString("UID=jane_doe", conn_map);

    ConnectionStringHelper::MergeBaseConnectionString(base_conn_map, conn_map);

    EXPECT_EQ("PWD={a+b};SSLMODE=require;UID=jane_doe", ConnectionStringHelper::BuildMinimumConnectionString(conn_map));
}

// Same MSVC STL limit as MaskConnectionStringLongQuotedValue, the pattern is shared
TEST_F(ConnectionStringHelperTest, RemoveInternalWrapperKeysLongQuotedValue) {
    const std::string conn_str(std::string(KEY_RDS_TEST_CONN) + "={" + std::string(4096, 'a') + "};UID=jane_doe");

    std::string result;
    EXPECT_NO_THROW(result = ConnectionStringHelper::RemoveInternalWrapperKeys(conn_str));
    EXPECT_EQ("UID=jane_doe", result);
}

TEST_F(ConnectionStringHelperTest, MaskConnectionStringSeveralQuotedValues) {
    const std::string conn_str("IDP_PASSWORD={c;d}}};PWD={a}}b};UID=jane_doe");

    EXPECT_EQ("IDP_PASSWORD=[REDACTED];PWD=[REDACTED];UID=jane_doe", ConnectionStringHelper::MaskSensitiveInformation(conn_str));
}
