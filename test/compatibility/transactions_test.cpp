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
#include <thread>

#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/test_utils.h"

static std::string txn_test_server;
static int txn_test_port;
static std::string txn_test_dsn;
static std::string txn_test_db;
static std::string txn_test_uid;
static std::string txn_test_pwd;
static std::string txn_test_base_driver;
static std::string txn_test_base_dsn;

class ConcurrentTransactionsTest : public testing::TestWithParam<std::string> {
protected:
    static constexpr int NUM_CONNECTIONS = 3;

    std::string DROP_TABLE_QUERY = "DROP TABLE IF EXISTS concurrent_test";
    std::string CREATE_TABLE_QUERY = "CREATE TABLE concurrent_test (id SERIAL PRIMARY KEY, thread_id INT, transaction_id INT, data VARCHAR(255), created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";
    std::string COUNT_QUERY = "SELECT COUNT(*) FROM concurrent_test";

    std::string conn_str = "";
    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;

    void SetUp() override {
        // Workaround for log creation
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::string test_dsn = GetParam();
        ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env));
        ASSERT_EQ(SQL_SUCCESS, SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
        ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc));

        conn_str = ConnectionStringBuilder(test_dsn.c_str(), txn_test_server, txn_test_port)
            .withUID(txn_test_uid)
            .withPWD(txn_test_pwd)
            .withDatabase(txn_test_db)
            .withBaseDriver(txn_test_base_driver)
            .withBaseDSN(txn_test_base_dsn)
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

        // Allow ng-log to flush before ShutdownLogging() is triggered by freeing the env handle.
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ODBC_HELPER::CleanUpHandles(env, dbc, nullptr);
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

TEST_P(ConcurrentTransactionsTest, CommitWithDBCHandle) {
    // Open two connections on the same env, insert on both with autocommit off,
    // but only commit one using SQLEndTran(SQL_HANDLE_DBC). Verify that only
    // the committed connection's row is visible.
    SQLHDBC dbc1, dbc2;
    SQLHSTMT stmt1, stmt2;
    SQLRETURN rc;

    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc1);
    SQLSetConnectAttr(dbc1, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
    rc = ODBC_HELPER::DriverConnect(dbc1, conn_str);
    ASSERT_TRUE(SQL_SUCCEEDED(rc));
    SQLAllocHandle(SQL_HANDLE_STMT, dbc1, &stmt1);

    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc2);
    SQLSetConnectAttr(dbc2, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
    rc = ODBC_HELPER::DriverConnect(dbc2, conn_str);
    ASSERT_TRUE(SQL_SUCCEEDED(rc));
    SQLAllocHandle(SQL_HANDLE_STMT, dbc2, &stmt2);

    // Insert one row on each connection
    rc = ODBC_HELPER::ExecuteQuery(stmt1, "INSERT INTO concurrent_test (thread_id, transaction_id, data) VALUES (1, 1, 'dbc1')");
    EXPECT_TRUE(SQL_SUCCEEDED(rc));
    rc = ODBC_HELPER::ExecuteQuery(stmt2, "INSERT INTO concurrent_test (thread_id, transaction_id, data) VALUES (2, 1, 'dbc2')");
    EXPECT_TRUE(SQL_SUCCEEDED(rc));

    // Neither row should be visible yet
    EXPECT_EQ(0, GetRowCount());

    // Commit only dbc1
    rc = SQLEndTran(SQL_HANDLE_DBC, dbc1, SQL_COMMIT);
    EXPECT_TRUE(SQL_SUCCEEDED(rc));

    // Only dbc1's row should be visible
    int count_after_commit1 = GetRowCount();
    EXPECT_EQ(1, count_after_commit1);

    // Rollback dbc2
    rc = SQLEndTran(SQL_HANDLE_DBC, dbc2, SQL_ROLLBACK);
    EXPECT_TRUE(SQL_SUCCEEDED(rc));

    // Still only 1 row
    EXPECT_EQ(1, GetRowCount());

    SQLFreeHandle(SQL_HANDLE_STMT, stmt1);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt2);
    SQLDisconnect(dbc1);
    SQLDisconnect(dbc2);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc1);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc2);
}

TEST_P(ConcurrentTransactionsTest, CommitWithEnvHandle) {
    // Execute transactions and commit using the ENV handle.
    SQLHDBC multi_dbc[NUM_CONNECTIONS];
    SQLHSTMT multi_stmt[NUM_CONNECTIONS];

    SQLRETURN rc = SQL_ERROR;

    for (int i = 0; i < NUM_CONNECTIONS; ++i) {
        SQLAllocHandle(SQL_HANDLE_DBC, env, &multi_dbc[i]);
        SQLSetConnectAttr(multi_dbc[i], SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
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
    fprintf(stderr, "[DEBUG] CommitWithEnvHandle in test SQLEndTran(ENV) rc=%d (DSN=%s)\n", rc, GetParam().c_str());
    fflush(stderr);
    EXPECT_TRUE(SQL_SUCCEEDED(rc));

    EXPECT_EQ(NUM_CONNECTIONS, GetRowCount());

    for (int i = 0; i < NUM_CONNECTIONS; ++i) {
        SQLFreeHandle(SQL_HANDLE_STMT, multi_stmt[i]);
        SQLDisconnect(multi_dbc[i]);
        SQLFreeHandle(SQL_HANDLE_DBC, multi_dbc[i]);
    }
}

static std::vector<std::string> getTxnDsnValues() {
    txn_test_server = TEST_UTILS::GetEnvVar("TEST_SERVER", "localhost");
    std::string port_str = TEST_UTILS::GetEnvVar("TEST_PORT", "5432");
    txn_test_port = std::strtol(port_str.c_str(), nullptr, 0);

    txn_test_dsn = TEST_UTILS::GetEnvVar("TEST_DSN", "wrapper-dsn");
    txn_test_db = TEST_UTILS::GetEnvVar("TEST_DATABASE");
    txn_test_uid = TEST_UTILS::GetEnvVar("TEST_USERNAME");
    txn_test_pwd = TEST_UTILS::GetEnvVar("TEST_PASSWORD");
    txn_test_base_driver = TEST_UTILS::GetEnvVar("TEST_BASE_DRIVER");
    txn_test_base_dsn = TEST_UTILS::GetEnvVar("TEST_BASE_DSN");

    return std::vector<std::string> {
        txn_test_dsn,
#ifdef TEST_BASE_OUTPUT
        txn_test_base_dsn
#endif
    };
}

INSTANTIATE_TEST_SUITE_P(
    ConcurrentTransactions,
    ConcurrentTransactionsTest,
    testing::ValuesIn(getTxnDsnValues())
);
