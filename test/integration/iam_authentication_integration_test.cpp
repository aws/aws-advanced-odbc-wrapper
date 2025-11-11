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

#include "../common/connection_string_builder.h"
#include "integration_test_utils.h"

class IamAuthenticationIntegrationTest : public testing::Test {
protected:
    std::string auth_type = "IAM";
    std::string test_dsn = "unicode_dsn";
    std::string test_db = "postgres";
    std::string test_uid = "iam_user";
    std::string test_pwd = "password";
    int test_port = 5432;
    std::string test_region = "us-east-2";
    std::string test_server = "database-pg-ulojonat.cluster-cx422ywmsto6.us-east-2.rds.amazonaws.com";

    std::string test_iam_user = "iam_user";

    RDS_STR connection_string = TEXT("");

    SQLHENV env = nullptr;
    SQLHDBC dbc = nullptr;

    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    void SetUp() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
        SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
        SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    }

    void TearDown() override {
        if (nullptr != dbc) {
            SQLFreeHandle(SQL_HANDLE_DBC, dbc);
        }
        if (nullptr != env) {
            SQLFreeHandle(SQL_HANDLE_ENV, env);
        }
    }
};

// Tests a simple IAM connection with all expected fields provided.
TEST_F(IamAuthenticationIntegrationTest, SimpleIamConnection) {
    connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_iam_user)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .getRdsString();

    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that IAM connection will still connect
// when given an IP address instead of a cluster name.
TEST_F(IamAuthenticationIntegrationTest, ConnectToIpAddress) {
    GTEST_SKIP() << "Required to set IAM Host, not yet implemented";

    auto ip_address = INTEGRATION_TEST_UTILS::host_to_IP(test_server);
    connection_string = ConnectionStringBuilder(test_dsn, ip_address, test_port)
        .withUID(test_iam_user)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .getRdsString();

    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that IAM connection will still connect
// when given a wrong password (because the password gets replaced by the auth token).
TEST_F(IamAuthenticationIntegrationTest, WrongPassword) {
    connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_iam_user)
        .withPWD("GARBAGE-PASSWORD")
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .getRdsString();

    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that the IAM connection will fail when provided a wrong user.
TEST_F(IamAuthenticationIntegrationTest, WrongUser) {
    auto connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID("WRONG_USER")
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .getRdsString();

    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, rc);

    SQLSMALLINT stmt_length;
    SQLINTEGER native_err;
    SQLTCHAR msg[SQL_MAX_MESSAGE_LENGTH] = {0}, state[6] = {0};
    rc = SQLError(nullptr, dbc, nullptr, state, &native_err, msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
    EXPECT_EQ(SQL_SUCCESS, rc);
    EXPECT_STREQ(AS_SQLTCHAR(SQL_ERR_UNABLE_TO_CONNECT), state);
}

// Tests that the IAM connection will fail when provided an empty user.
TEST_F(IamAuthenticationIntegrationTest, EmptyUser) {
    auto connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID("")
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(true)
        .withSslMode("allow")
        .getRdsString();

    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, rc);

    SQLSMALLINT stmt_length;
    SQLINTEGER native_err;
    SQLTCHAR msg[SQL_MAX_MESSAGE_LENGTH] = {0}, state[6] = {0};
    rc = SQLError(nullptr, dbc, nullptr, state, &native_err, msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
    EXPECT_EQ(SQL_SUCCESS, rc);
    EXPECT_STREQ(AS_SQLTCHAR(SQL_ERR_UNABLE_TO_CONNECT), state);
}
