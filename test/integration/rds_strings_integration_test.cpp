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

class RdsStringsIntegrationTest : public BaseConnectionTest {
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
};

TEST_F(RdsStringsIntegrationTest, SpecialCharacterQuery) {
    std::string conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .getString();
    ASSERT_EQ(SQL_SUCCESS, ODBC_HELPER::DriverConnect(dbc, conn_str));

    SQLHSTMT hstmt = SQL_NULL_HANDLE;
    ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt));

    ODBC_HELPER::ExecuteQuery(hstmt, "DROP TABLE IF EXISTS test_table");
    ODBC_HELPER::ExecuteQuery(hstmt, "CREATE TABLE test_table (id INT PRIMARY KEY, name VARCHAR(255))");

    ODBC_HELPER::ExecuteQuery(hstmt, "INSERT INTO test_table VALUES (1, 'é')");
    ODBC_HELPER::ExecuteQuery(hstmt, "INSERT INTO test_table VALUES (2, '日本語')");

    // Read it back and verify
    ASSERT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(hstmt, "SELECT name FROM test_table WHERE id = 1"));
    ASSERT_EQ(SQL_SUCCESS, SQLFetch(hstmt));

    SQLTCHAR buf[512] = {0};
    SQLLEN buflen;
    ASSERT_EQ(SQL_SUCCESS, SQLGetData(hstmt, 1, SQL_C_TCHAR, buf, sizeof(buf), &buflen));

    std::string result = STRING_HELPER::SqltcharToAnsi(buf);
    EXPECT_EQ(result, "é");

    SQLCloseCursor(hstmt);

    // Query using non-ASCII in WHERE clause
    ASSERT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(hstmt, "SELECT id FROM test_table WHERE name = '日本語'"));
    ASSERT_EQ(SQL_SUCCESS, SQLFetch(hstmt));

    SQLINTEGER id = 0;
    ASSERT_EQ(SQL_SUCCESS, SQLGetData(hstmt, 1, SQL_INTEGER, &id, sizeof(id), &buflen));
    EXPECT_EQ(id, 2);

    ODBC_HELPER::ExecuteQuery(hstmt, "DROP TABLE IF EXISTS test_table");
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}
