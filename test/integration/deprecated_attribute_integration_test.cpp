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

#include "../../driver/util/windows_headers.h"

#include <sql.h>
#include <sqlext.h>

#include <string>

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"

class DeprecatedAttributeIntegrationTest : public BaseConnectionTest {
protected:
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

    std::string BaseConnStr() const {
        return ConnectionStringBuilder(test_dsn, test_server, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .getString();
    }
};

TEST_F(DeprecatedAttributeIntegrationTest, ExtraUrlEncodeReportsDeprecationWarning) {
    const std::string conn_str = BaseConnStr() + "EXTRA_URL_ENCODE=1;";

    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = {0};
    STRING_HELPER::AnsiToUnicode(conn_str.c_str(), conn_str_in);
    ASSERT_EQ(SQL_SUCCESS_WITH_INFO,
        SQLDriverConnect(dbc, nullptr, conn_str_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));

    SQLTCHAR sql_state[6] = {0};
    SQLTCHAR message[SQL_MAX_MESSAGE_LENGTH] = {0};
    SQLINTEGER native_error = 0;
    SQLSMALLINT message_len = 0;
    ASSERT_EQ(SQL_SUCCESS,
        SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 1, sql_state, &native_error, message, SQL_MAX_MESSAGE_LENGTH, &message_len));

    EXPECT_EQ(std::string(SQL_ERR_INVALID_PARAMETER), std::string(STRING_HELPER::SqltcharToAnsi(sql_state)));
    const std::string message_str = STRING_HELPER::SqltcharToAnsi(message);
    EXPECT_NE(std::string::npos, message_str.find("EXTRA_URL_ENCODE"));
    EXPECT_NE(std::string::npos, message_str.find("Deprecated"));

    // The connection itself is usable, the attribute is only reported, never fatal
    EXPECT_TRUE(ODBC_HELPER::TestSimpleQuery(dbc));
}

// Without the deprecated attribute a healthy connection stays SQL_SUCCESS
TEST_F(DeprecatedAttributeIntegrationTest, NoWarningWithoutDeprecatedAttribute) {
    ASSERT_EQ(SQL_SUCCESS, ODBC_HELPER::DriverConnect(dbc, BaseConnStr()));
    EXPECT_TRUE(ODBC_HELPER::TestSimpleQuery(dbc));
}
