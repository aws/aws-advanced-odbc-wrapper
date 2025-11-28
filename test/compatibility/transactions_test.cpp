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

#include <atomic>
#include <chrono>
#include <future>
#include <string>

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

class ConcurrentTransactionsTest: public BaseConnectionTest {
protected:
    static constexpr int NUM_CONNECTIONS = 3;

    std::string DROP_TABLE_QUERY = "DROP TABLE IF EXISTS concurrent_test";
    std::string CREATE_TABLE_QUERY = "CREATE TABLE concurrent_test (id SERIAL PRIMARY KEY, thread_id INT, transaction_id INT, data VARCHAR(255), created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";
    std::string COUNT_QUERY = "SELECT COUNT(*) FROM concurrent_test";

    static void SetUpTestSuite() {
        BaseConnectionTest::SetUpTestSuite();
    }

    static void TearDownTestSuite() {
        BaseConnectionTest::TearDownTestSuite();
    }

    void SetUp() override {
        BaseConnectionTest::SetUp();
        conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .getString();

        // Create test table "concurrent_test".
        SQLHDBC setup_dbc;
        SQLAllocHandle(SQL_HANDLE_DBC, env, &setup_dbc);
        SQLRETURN rc = ODBC_HELPER::DriverConnect(setup_dbc, conn_str);
        EXPECT_EQ(SQL_SUCCESS, rc);

        SQLHSTMT setup_stmt;
        SQLAllocHandle(SQL_HANDLE_STMT, setup_dbc, &setup_stmt);
        ODBC_HELPER::ExecuteQuery(setup_stmt, DROP_TABLE_QUERY);
        rc = ODBC_HELPER::ExecuteQuery(setup_stmt, CREATE_TABLE_QUERY);
        EXPECT_EQ(SQL_SUCCESS, rc);

        SQLFreeHandle(SQL_HANDLE_STMT, setup_stmt);
        SQLDisconnect(setup_dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, setup_dbc);
    }

    void TearDown() override {
        // Drop test table "concurrent_test".
        SQLHDBC cleanup_dbc;
        SQLAllocHandle(SQL_HANDLE_DBC, env, &cleanup_dbc);
        SQLRETURN rc = ODBC_HELPER::DriverConnect(cleanup_dbc, conn_str);
        EXPECT_EQ(SQL_SUCCESS, rc);

        SQLHSTMT cleanup_stmt;
        SQLAllocHandle(SQL_HANDLE_STMT, cleanup_dbc, &cleanup_stmt);
        ODBC_HELPER::ExecuteQuery(cleanup_stmt, DROP_TABLE_QUERY);

        SQLFreeHandle(SQL_HANDLE_STMT, cleanup_stmt);
        SQLDisconnect(cleanup_dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, cleanup_dbc);

        BaseConnectionTest::TearDown();
    }

    int GetRowCount() {
        SQLHDBC verify_dbc;
        SQLAllocHandle(SQL_HANDLE_DBC, env, &verify_dbc);
        SQLRETURN rc = ODBC_HELPER::DriverConnect(verify_dbc, conn_str);
        EXPECT_EQ(SQL_SUCCESS, rc);

        SQLHSTMT verify_stmt;
        SQLAllocHandle(SQL_HANDLE_STMT, verify_dbc, &verify_stmt);
        ODBC_HELPER::ExecuteQuery(verify_stmt, COUNT_QUERY);

        SQLFetch(verify_stmt);
        SQLINTEGER count;
        SQLLEN indicator;
        SQLGetData(verify_stmt, 1, SQL_C_SLONG, &count, 0, &indicator);

        SQLFreeHandle(SQL_HANDLE_STMT, verify_stmt);
        SQLDisconnect(verify_dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, verify_dbc);

        return count;
    }
};

TEST_F(ConcurrentTransactionsTest, CommitWithDBCHandle) {
    // Execute a single transaction and commit using the DBC handle.
    SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, 0);
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    ASSERT_TRUE(SQL_SUCCEEDED(rc));

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    std::string insert_query = "INSERT INTO concurrent_test (thread_id, transaction_id, data) VALUES (1, 1, 'test')";
    rc = ODBC_HELPER::ExecuteQuery(stmt, insert_query);
    EXPECT_TRUE(SQL_SUCCEEDED(rc));

    // Verify the table is still empty.
    EXPECT_EQ(0, GetRowCount());

    rc = SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);
    EXPECT_TRUE(SQL_SUCCEEDED(rc));

    // Verify the row was inserted
    EXPECT_EQ(1, GetRowCount());

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

TEST_F(ConcurrentTransactionsTest, CommitWithEnvHandle) {
    // Execute transactions and commit using the ENV handle.
    SQLHDBC multi_dbc[NUM_CONNECTIONS];
    SQLHSTMT multi_stmt[NUM_CONNECTIONS];

    SQLRETURN rc = SQL_ERROR;

    for (int i = 0; i < NUM_CONNECTIONS; ++i) {
        SQLAllocHandle(SQL_HANDLE_DBC, env, &multi_dbc[i]);
        SQLSetConnectAttr(multi_dbc[i], SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
        rc = ODBC_HELPER::DriverConnect(multi_dbc[i], conn_str);
        EXPECT_TRUE(SQL_SUCCEEDED(rc));
        SQLAllocHandle(SQL_HANDLE_STMT, multi_dbc[i], &multi_stmt[i]);
    }

    std::string begin_query = "BEGIN;";
    for (int i = 0; i < NUM_CONNECTIONS; ++i) {
        rc = ODBC_HELPER::ExecuteQuery(multi_stmt[i], begin_query);
        EXPECT_TRUE(SQL_SUCCEEDED(rc));

        std::string insert_query = "INSERT INTO concurrent_test (thread_id, transaction_id, data) VALUES (" +
            std::to_string(i + 1) + ", 1, 'dbc" + std::to_string(i + 1) + "')";
        rc = ODBC_HELPER::ExecuteQuery(multi_stmt[i], insert_query);
        EXPECT_TRUE(SQL_SUCCEEDED(rc));
    }

    // Verify the table is still empty.
    EXPECT_EQ(0, GetRowCount());

    rc = SQLEndTran(SQL_HANDLE_ENV, env, SQL_COMMIT);
    EXPECT_TRUE(SQL_SUCCEEDED(rc));

    // Verify all rows were inserted
    EXPECT_EQ(NUM_CONNECTIONS, GetRowCount());

    for (int i = 0; i < NUM_CONNECTIONS; ++i) {
        SQLFreeHandle(SQL_HANDLE_STMT, multi_stmt[i]);
        SQLDisconnect(multi_dbc[i]);
        SQLFreeHandle(SQL_HANDLE_DBC, multi_dbc[i]);
    }
}
