#include <gtest/gtest.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>

#include "../common/connection_string_builder.h"

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

TEST_F(ConnectionTest, SQLDriverConnect_BaseDSN_Success) {
    GTEST_SKIP() << "Needs DSN parser to get Driver from Base DSN";

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

TEST_F(ConnectionTest, SQLConnect_BaseDriver_Success) {
    GTEST_SKIP() << "SQLConnect is not implemented yet. Needs DSN parser";

    SQLRETURN ret = 0;
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv));
    EXPECT_EQ(SQL_SUCCESS, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

    EXPECT_EQ(SQL_SUCCESS, SQLConnect(hdbc,
        AS_SQLTCHAR(test_dsn), SQL_NTS,
        AS_SQLTCHAR(test_uid), SQL_NTS,
        AS_SQLTCHAR(test_pwd), SQL_NTS)
    );

    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_ENV, henv));
}
