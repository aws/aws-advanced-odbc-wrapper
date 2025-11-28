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

#include <sql.h>
#include <sqlext.h>

#include <string>

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

class SecretsManagerIntegrationTest : public BaseConnectionTest {
protected:
    std::string auth_type = "SECRETS_MANAGER";
    std::string test_secret_arn = TEST_UTILS::GetEnvVar("TEST_SECRET_ARN");

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

TEST_F(SecretsManagerIntegrationTest, EnableSecretsManagerWithRegion) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withSecretId(test_secret_arn)
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

TEST_F(SecretsManagerIntegrationTest, EnableSecretsManagerWithoutRegion) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withSecretId(test_secret_arn)
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Passing in a wrong region should still work in retrieving secrets
// A full secret ARN will contain the proper region
TEST_F(SecretsManagerIntegrationTest, EnableSecretsManagerWrongRegion) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion("us-fake-1")
        .withSecretId(test_secret_arn)
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

TEST_F(SecretsManagerIntegrationTest, EnableSecretsManagerInvalidSecretID) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withSecretId("invalid-id")
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_ERROR, rc);

    // Check state
    SQLTCHAR msg[SQL_MAX_MESSAGE_LENGTH] = {0}, state[6] = {0};
    SQLINTEGER native_error = 0;
    SQLSMALLINT stmt_length;
    EXPECT_EQ(SQL_SUCCESS, SQLError(nullptr, dbc, nullptr, state, &native_error, msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length));
    EXPECT_STREQ(SQL_ERR_INVALID_PARAMETER, STRING_HELPER::SqltcharToAnsi(state));
}
