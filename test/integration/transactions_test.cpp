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

#include "../common/connection_string_builder.h"
#include "integration_test_utils.h"

class ConcurrentTransactionsTest: public testing::Test {
    protected:
        static constexpr int NUM_CONNECTIONS = 3;
        std::string test_dsn = std::getenv("TEST_DSN");
        std::string test_db = std::getenv("TEST_DATABASE");
        std::string test_uid = std::getenv("TEST_USERNAME");
        std::string test_pwd = std::getenv("TEST_PASSWORD");
        int test_port =
            INTEGRATION_TEST_UTILS::str_to_int(INTEGRATION_TEST_UTILS::get_env_var("TEST_PORT", (char *) "5432"));
        std::string test_server = std::getenv("TEST_SERVER");

        std::string connection_string;
        SQLHENV env = nullptr;

        SQLTCHAR *DROP_TABLE_QUERY = AS_SQLTCHAR("DROP TABLE IF EXISTS concurrent_test");
        SQLTCHAR *CREATE_TABLE_QUERY =
            AS_SQLTCHAR("CREATE TABLE concurrent_test (id SERIAL PRIMARY KEY, thread_id INT, transaction_id INT, data "
                        "VARCHAR(255), created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");
        SQLTCHAR *COUNT_QUERY = AS_SQLTCHAR("SELECT COUNT(*) FROM concurrent_test");

        void SetUp() override {
            connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
                                    .withUID(test_uid)
                                    .withPWD(test_pwd)
                                    .withDatabase(test_db)
                                    .getRdsString();

            SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
            SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);

            // Create test table "concurrent_test".
            SQLHDBC setup_dbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &setup_dbc);
            SQLDriverConnect(setup_dbc,
                             nullptr,
                             AS_SQLTCHAR(connection_string.c_str()),
                             SQL_NTS,
                             nullptr,
                             0,
                             nullptr,
                             SQL_DRIVER_NOPROMPT);

            SQLHSTMT setup_stmt;
            SQLAllocHandle(SQL_HANDLE_STMT, setup_dbc, &setup_stmt);
            SQLExecDirect(setup_stmt, DROP_TABLE_QUERY, SQL_NTS);
            EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(setup_stmt, CREATE_TABLE_QUERY, SQL_NTS));

            SQLFreeHandle(SQL_HANDLE_STMT, setup_stmt);
            SQLDisconnect(setup_dbc);
            SQLFreeHandle(SQL_HANDLE_DBC, setup_dbc);
        }

        void TearDown() override {
            // Drop test table "concurrent_test".
            SQLHDBC cleanup_dbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &cleanup_dbc);
            SQLDriverConnect(cleanup_dbc,
                             nullptr,
                             AS_SQLTCHAR(connection_string.c_str()),
                             SQL_NTS,
                             nullptr,
                             0,
                             nullptr,
                             SQL_DRIVER_NOPROMPT);

            SQLHSTMT cleanup_stmt;
            SQLAllocHandle(SQL_HANDLE_STMT, cleanup_dbc, &cleanup_stmt);
            SQLExecDirect(cleanup_stmt, DROP_TABLE_QUERY, SQL_NTS);

            SQLFreeHandle(SQL_HANDLE_STMT, cleanup_stmt);
            SQLDisconnect(cleanup_dbc);
            SQLFreeHandle(SQL_HANDLE_DBC, cleanup_dbc);

            if (nullptr != env) {
                SQLFreeHandle(SQL_HANDLE_ENV, env);
            }
        }

        int GetRowCount() {
            SQLHDBC verify_dbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &verify_dbc);
            SQLDriverConnect(verify_dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
            
            SQLHSTMT verify_stmt;
            SQLAllocHandle(SQL_HANDLE_STMT, verify_dbc, &verify_stmt);
            SQLExecDirect(verify_stmt, COUNT_QUERY, SQL_NTS);
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

    SQLHDBC dbc;
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, 0);

    SQLRETURN ret = SQLDriverConnect(
        dbc, nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    ret = SQLExecDirect(
        stmt,
        AS_SQLTCHAR("INSERT INTO concurrent_test (thread_id, transaction_id, data) VALUES (1, 1, 'test')"),
        SQL_NTS);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));

    // Verify the table is still empty.
    ASSERT_EQ(0, GetRowCount());

    ret = SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));

    // Verify the row was inserted
    ASSERT_EQ(1, GetRowCount());

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
}

TEST_F(ConcurrentTransactionsTest, CommitWithEnvHandle) {
    // Execute three transactions and commit using the ENV handle.

    SQLHDBC dbc[NUM_CONNECTIONS];
    SQLHSTMT stmt[NUM_CONNECTIONS];

    for (int i = 0; i < NUM_CONNECTIONS; ++i) {
        SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc[i]);
        SQLSetConnectAttr(dbc[i], SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
        SQLRETURN ret = SQLDriverConnect(
            dbc[i], nullptr, AS_SQLTCHAR(connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        ASSERT_TRUE(SQL_SUCCEEDED(ret));
        SQLAllocHandle(SQL_HANDLE_STMT, dbc[i], &stmt[i]);
    }

    for (int i = 0; i < NUM_CONNECTIONS; ++i) {
        SQLRETURN begin_ret = SQLExecDirect(stmt[i], AS_SQLTCHAR("BEGIN"), SQL_NTS);
        ASSERT_TRUE(SQL_SUCCEEDED(begin_ret));

        std::string sql = "INSERT INTO concurrent_test (thread_id, transaction_id, data) VALUES (" +
            std::to_string(i + 1) + ", 1, 'dbc" + std::to_string(i + 1) + "')";
        SQLRETURN ret = SQLExecDirect(stmt[i], AS_SQLTCHAR(sql.c_str()), SQL_NTS);
        ASSERT_TRUE(SQL_SUCCEEDED(ret));
    }

    // Verify the table is still empty.
    ASSERT_EQ(0, GetRowCount());

    SQLRETURN commit_ret = SQLEndTran(SQL_HANDLE_ENV, env, SQL_COMMIT);
    ASSERT_TRUE(SQL_SUCCEEDED(commit_ret));

    // Verify all rows were inserted
    ASSERT_EQ(NUM_CONNECTIONS, GetRowCount());

    for (int i = 0; i < NUM_CONNECTIONS; ++i) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt[i]);
        SQLDisconnect(dbc[i]);
        SQLFreeHandle(SQL_HANDLE_DBC, dbc[i]);
    }
}
