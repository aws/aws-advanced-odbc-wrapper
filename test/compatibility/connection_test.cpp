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

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

class ConnectionTest : public BaseConnectionTest {
protected:
    std::string test_base_driver = TEST_UTILS::GetEnvVar("TEST_BASE_DRIVER");
    std::string test_base_dsn = TEST_UTILS::GetEnvVar("TEST_BASE_DSN");

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
private:
};

TEST_F(ConnectionTest, SQLDriverConnect_BaseDriver_Success) {
    SQLRETURN ret = SQL_SUCCESS;
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv));
    EXPECT_EQ(SQL_SUCCESS, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

    ConnectionStringBuilder builder = ConnectionStringBuilder(test_dsn, test_server, test_port);
    conn_str = builder.withDatabase(test_db)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withBaseDriver(test_base_driver)
        .getString();
    ret = ODBC_HELPER::DriverConnect(hdbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, ret);

    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_ENV, henv));
}

TEST_F(ConnectionTest, SQLDriverConnect_BaseDsn_Success) {
    SQLRETURN ret = SQL_SUCCESS;
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv));
    EXPECT_EQ(SQL_SUCCESS, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

    ConnectionStringBuilder builder = ConnectionStringBuilder(test_dsn, test_server, test_port);
    conn_str = builder.withDatabase(test_db)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withBaseDSN(test_base_dsn)
        .getString();
    ret = ODBC_HELPER::DriverConnect(hdbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, ret);

    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_ENV, henv));
}

TEST_F(ConnectionTest, SQLDriverConnect_BaseDriverAndDsn_Success) {
    SQLRETURN ret = SQL_SUCCESS;
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv));
    EXPECT_EQ(SQL_SUCCESS, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

    ConnectionStringBuilder builder = ConnectionStringBuilder(test_dsn, test_server, test_port);
    conn_str = builder.withDatabase(test_db)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withBaseDriver(test_base_driver)
        .withBaseDSN(test_base_dsn)
        .getString();
    ret = ODBC_HELPER::DriverConnect(hdbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, ret);

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

    ret = ODBC_HELPER::DsnConnect(hdbc, test_dsn, test_uid, test_pwd);
    EXPECT_EQ(SQL_SUCCESS, ret);

    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_ENV, henv));
}
