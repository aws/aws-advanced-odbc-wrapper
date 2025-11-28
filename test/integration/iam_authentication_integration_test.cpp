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

#include <gtest/gtest.h>

#ifdef WIN32
    #include <windows.h>
#endif

#include <thread>
#include <chrono>
#include <sql.h>
#include <sqlext.h>

#include <string>

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

class IamAuthenticationIntegrationTest : public BaseConnectionTest {
protected:
    std::string auth_type = "IAM";
    std::string test_iam_user = TEST_UTILS::GetEnvVar("TEST_IAM_USER", "");

    static void SetUpTestSuite() {
        BaseConnectionTest::SetUpTestSuite();
    }

    static void TearDownTestSuite() {
        BaseConnectionTest::TearDownTestSuite();
    }

    void SetUp() override {
        BaseConnectionTest::SetUp();
    }

    void TearDown() override {
        BaseConnectionTest::TearDown();
    }
};

// Tests a simple IAM connection with all expected fields provided.
TEST_F(IamAuthenticationIntegrationTest, SimpleIamConnection) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_iam_user)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that IAM connection will still connect
// when given an IP address instead of a cluster name.
TEST_F(IamAuthenticationIntegrationTest, ConnectToIpAddress) {
    std::string ip_address = TEST_UTILS::HostToIp(test_server);
    conn_str = ConnectionStringBuilder(test_dsn, ip_address, test_port)
        .withUID(test_iam_user)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .withIamHost(test_server)
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that IAM connection will still connect
// when given a wrong password (because the password gets replaced by the auth token).
TEST_F(IamAuthenticationIntegrationTest, WrongPassword) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_iam_user)
        .withPWD("GARBAGE-PASSWORD")
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that the IAM connection will fail when provided a wrong user.
TEST_F(IamAuthenticationIntegrationTest, WrongUser) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID("WRONG_USER")
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_ERROR, rc);

    SQLSMALLINT stmt_length;
    SQLINTEGER native_err;
    SQLTCHAR msg[SQL_MAX_MESSAGE_LENGTH] = {0}, state[6] = {0};
    rc = SQLError(nullptr, dbc, nullptr, state, &native_err, msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
    EXPECT_EQ(SQL_SUCCESS, rc);
    EXPECT_STREQ(SQL_ERR_UNABLE_TO_CONNECT, STRING_HELPER::SqltcharToAnsi(state));
}

// Tests that the IAM connection will fail when provided an empty user.
TEST_F(IamAuthenticationIntegrationTest, EmptyUser) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID("")
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_ERROR, rc);

    SQLSMALLINT stmt_length;
    SQLINTEGER native_err;
    SQLTCHAR msg[SQL_MAX_MESSAGE_LENGTH] = {0}, state[6] = {0};
    rc = SQLError(nullptr, dbc, nullptr, state, &native_err, msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
    EXPECT_EQ(SQL_SUCCESS, rc);
    EXPECT_STREQ(SQL_ERR_UNABLE_TO_CONNECT, STRING_HELPER::SqltcharToAnsi(state));
}
