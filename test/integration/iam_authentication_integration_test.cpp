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
    // Set TEST_SKIP_IAM=1 for drivers that can't do IAM (e.g. MySQL Connector/ODBC).
    std::string test_skip_iam = TEST_UTILS::GetEnvVar("TEST_SKIP_IAM", "");
    // psqlODBC needs the token's '%' double-encoded to survive its connection-string parse, but
    // MySQL/MariaDB ODBC consume the password verbatim, so double-encoding corrupts the token and
    // the server rejects it (28000). Only apply extra URL encoding for non-MySQL dialects.
    bool extra_url_encode = (test_dialect != "AURORA_MYSQL");

    static void SetUpTestSuite() {
        BaseConnectionTest::SetUpTestSuite();
    }

    static void TearDownTestSuite() {
        BaseConnectionTest::TearDownTestSuite();
    }

    void SetUp() override {
        if (test_skip_iam == "1" || test_skip_iam == "true") {
            GTEST_SKIP() << "Skipping IAM tests: the configured underlying driver does not support "
                            "IAM authentication (TEST_SKIP_IAM is set).";
        }
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
        .withExtraUrlEncode(extra_url_encode)
        .withSslMode("allow")
        .withClusterId("SimpleIamConnection")
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
    ASSERT_FALSE(ip_address.empty()) << "Failed to resolve IP address for host: " << test_server;
    ConnectionStringBuilder builder(test_dsn, ip_address, test_port);
    builder.withUID(test_iam_user)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(extra_url_encode)
        .withSslMode("allow")
        .withIamHost(test_server)
        .withClusterId("ConnectToIpAddress");
    // Connecting by IP can't match the cert CN, so skip CN verification on MySQL/MariaDB (TLS stays on).
    if (test_dialect == "AURORA_MYSQL") {
        builder.withSslVerify(false);
    }
    conn_str = builder.getString();

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
        .withExtraUrlEncode(extra_url_encode)
        .withSslMode("allow")
        .withClusterId("WrongPassword")
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
        .withExtraUrlEncode(extra_url_encode)
        .withSslMode("allow")
        .withClusterId("WrongUser")
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_ERROR, rc);

    SQLSMALLINT stmt_length;
    SQLINTEGER native_err;
    SQLTCHAR msg[SQL_MAX_MESSAGE_LENGTH] = {0}, state[6] = {0};
    rc = SQLError(nullptr, dbc, nullptr, state, &native_err, msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
    EXPECT_EQ(SQL_SUCCESS, rc);
    // psqlODBC reports access failures as 08001; MySQL/MariaDB ODBC reports 28000.
    const std::string actual_state = STRING_HELPER::SqltcharToAnsi(state);
    if (test_dialect == "AURORA_MYSQL") {
        EXPECT_EQ(SQL_ERR_MYSQL_ACCESS_DENIED, actual_state);
    } else {
        EXPECT_EQ(SQL_ERR_UNABLE_TO_CONNECT, actual_state);
    }
}
// Tests that the IAM connection will fail when provided an empty user.
// Rejected by the wrapper's pre-validation (08001) before the base driver, regardless of dialect.
TEST_F(IamAuthenticationIntegrationTest, EmptyUser) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID("")
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withAuthExpiration(900)
        .withExtraUrlEncode(extra_url_encode)
        .withSslMode("allow")
        .withClusterId("EmptyUser")
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
