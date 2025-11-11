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

class SecretsManagerIntegrationTest : public testing::Test {
protected:
    std::string auth_type = "SECRETS_MANAGER";
    std::string test_dsn = std::getenv("TEST_DSN");
    std::string test_db = std::getenv("TEST_DATABASE");
    int test_port = INTEGRATION_TEST_UTILS::str_to_int(
        INTEGRATION_TEST_UTILS::get_env_var("TEST_PORT", (char*) "5432"));
    std::string test_region = INTEGRATION_TEST_UTILS::get_env_var("TEST_REGION", (char*) "us-west-1");
    std::string test_server = std::getenv("TEST_SERVER");

    std::string test_secret_arn = std::getenv("TEST_SECRET_ARN");

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

TEST_F(SecretsManagerIntegrationTest, EnableSecretsManagerWithRegion) {
    connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withSecretId(test_secret_arn)
        .getRdsString();

    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, rc);
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(SecretsManagerIntegrationTest, EnableSecretsManagerWithoutRegion) {
    connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withSecretId(test_secret_arn)
        .getRdsString();

    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, rc);
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

// Passing in a wrong region should still work in retrieving secrets
// A full secret ARN will contain the proper region
TEST_F(SecretsManagerIntegrationTest, EnableSecretsManagerWrongRegion) {
    connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion("us-fake-1")
        .withSecretId(test_secret_arn)
        .getRdsString();

    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, rc);
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(SecretsManagerIntegrationTest, EnableSecretsManagerInvalidSecretID) {
    connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withDatabase(test_db)
        .withAuthMode(auth_type)
        .withAuthRegion(test_region)
        .withSecretId("invalid-id")
        .getRdsString();

    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, rc);

    // Check state
    SQLTCHAR sqlstate[6] = {0}, message[SQL_MAX_MESSAGE_LENGTH] = {0};
    SQLINTEGER native_error = 0;
    SQLSMALLINT stmt_length;
    EXPECT_EQ(SQL_SUCCESS, SQLError(nullptr, dbc, nullptr, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length));
    EXPECT_STREQ(AS_SQLTCHAR(SQL_ERR_INVALID_PARAMETER), sqlstate);
}
