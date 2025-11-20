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

#include "../common/connection_string_builder.h"
#include "../common/string_helper.h"

static char* test_server;
static int test_port;
static char* test_dsn;
static char* test_db;
static char* test_uid;
static char* test_pwd;

static char* test_base_driver;
static char* test_base_dsn;

class ConnectionTest : public testing::Test {
public:
protected:
    static void SetUpTestSuite() {
        test_server = std::getenv("TEST_SERVER");
        test_port = std::strtol(std::getenv("TEST_PORT"), nullptr, 10);
        test_dsn = std::getenv("TEST_DSN");
        test_db = std::getenv("TEST_DATABASE");
        test_uid = std::getenv("TEST_USERNAME");
        test_pwd = std::getenv("TEST_PASSWORD");

        test_base_driver = std::getenv("TEST_BASE_DRIVER");
        test_base_dsn = std::getenv("TEST_BASE_DSN");
    }

    static void TearDownTestSuite() {}
    void SetUp() override {}
    void TearDown() override {}

private:
};

TEST_F(ConnectionTest, SQLDriverConnect_BaseDriver_Success) {
    SQLRETURN ret = 0;
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv));
    EXPECT_EQ(SQL_SUCCESS, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

    ConnectionStringBuilder builder = ConnectionStringBuilder(test_dsn, test_server, test_port);
    std::string conn_str = builder.withDatabase(test_db)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withBaseDriver(test_base_driver)
        .getString();
    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(conn_str.c_str(), conn_str_in);

    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(hdbc,
        nullptr,
        conn_str_in,
        SQL_NTS,
        nullptr,
        0,
        nullptr,
        SQL_DRIVER_NOPROMPT)
    );

    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_ENV, henv));
}

TEST_F(ConnectionTest, SQLDriverConnect_BaseDSN_Success) {
    SQLRETURN ret = 0;
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv));
    EXPECT_EQ(SQL_SUCCESS, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

    ConnectionStringBuilder builder = ConnectionStringBuilder(test_dsn, test_server, test_port);
    std::string conn_str = builder.withDatabase(test_db)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withBaseDSN(test_base_dsn)
        .getString();
    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(conn_str.c_str(), conn_str_in);

    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(hdbc,
        nullptr,
        AS_SQLTCHAR(conn_str.c_str()),
        SQL_NTS,
        nullptr,
        0,
        nullptr,
        SQL_DRIVER_NOPROMPT)
    );

    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_ENV, henv));
}

TEST_F(ConnectionTest, SQLDriverConnect_BaseDriverAndDSN_Success) {
    SQLRETURN ret = 0;
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv));
    EXPECT_EQ(SQL_SUCCESS, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

    ConnectionStringBuilder builder = ConnectionStringBuilder(test_dsn, test_server, test_port);
    std::string conn_str = builder.withDatabase(test_db)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withBaseDriver(test_base_driver)
        .withBaseDSN(test_base_dsn)
        .getString();
    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(conn_str.c_str(), conn_str_in);

    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(hdbc,
        nullptr,
        conn_str_in,
        SQL_NTS,
        nullptr,
        0,
        nullptr,
        SQL_DRIVER_NOPROMPT)
    );

    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_ENV, henv));
}

TEST_F(ConnectionTest, SQLConnect_BaseDriver_Success) {
    SQLRETURN ret = 0;
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv));
    EXPECT_EQ(SQL_SUCCESS, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

    SQLTCHAR dsn_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(test_dsn, dsn_in);
    SQLTCHAR uid_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(test_uid, uid_in);
    SQLTCHAR pwd_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(test_pwd, pwd_in);

    EXPECT_EQ(SQL_SUCCESS, SQLConnect(hdbc,
        dsn_in, SQL_NTS,
        uid_in, SQL_NTS,
        pwd_in, SQL_NTS)
    );

    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_ENV, henv));
}
