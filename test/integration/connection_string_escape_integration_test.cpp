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

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"

#ifdef _WIN32
// Overrides values of the user DSN under test and restores them when destroyed
class UserDsnValues {
public:
    explicit UserDsnValues(const std::string& dsn) : key_path_("Software\\ODBC\\ODBC.INI\\" + dsn) {}

    ~UserDsnValues() {
        for (const auto& [name, previous] : previous_) {
            if (previous) {
                RegSetKeyValueA(HKEY_CURRENT_USER, key_path_.c_str(), name.c_str(), REG_SZ, previous->c_str(), static_cast<DWORD>(previous->size() + 1));
            } else {
                RegDeleteKeyValueA(HKEY_CURRENT_USER, key_path_.c_str(), name.c_str());
            }
        }
    }

    bool Exists() const {
        HKEY key;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, key_path_.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
            return false;
        }
        RegCloseKey(key);
        return true;
    }

    void Set(const std::string& name, const std::string& value) {
        if (!previous_.contains(name)) {
            previous_[name] = Get(name);
        }
        RegSetKeyValueA(HKEY_CURRENT_USER, key_path_.c_str(), name.c_str(), REG_SZ, value.c_str(), static_cast<DWORD>(value.size() + 1));
    }

private:
    std::optional<std::string> Get(const std::string& name) const {
        char buffer[1024] = {0};
        DWORD size = sizeof(buffer);
        if (RegGetValueA(HKEY_CURRENT_USER, key_path_.c_str(), name.c_str(), RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) {
            return std::nullopt;
        }
        return std::string(buffer);
    }

    std::string key_path_;
    std::map<std::string, std::optional<std::string>> previous_;
};
#endif

// How values containing special characters reach a PostgreSQL server through psqlODBC. psqlODBC URL-decodes
// an unbraced PWD, ConnSettings and pqopt value ('+' becomes a space, '%41' becomes 'A') and takes a braced
// value literally, so whether a value arrives enclosed in braces decides what the server receives.
class ConnectionStringEscapeIntegrationTest : public BaseConnectionTest {
protected:
    // Login role whose password contains characters psqlODBC URL-decodes
    static constexpr const char* ESCAPE_USER = "odbc_conn_str_escape_user";
    static constexpr const char* ESCAPE_PASSWORD = "Odbc+Pass%41word";
    // ESCAPE_PASSWORD URL-encoded, psqlODBC decodes it back to ESCAPE_PASSWORD
    static constexpr const char* ESCAPE_PASSWORD_URL_ENCODED = "Odbc%2BPass%2541word";

    void SetUp() override {
        BaseConnectionTest::SetUp();
        if (test_dialect.find("POSTGRESQL") == std::string::npos) {
            GTEST_SKIP() << "Requires psqlODBC, TEST_DIALECT is '" << test_dialect << "'";
        }

        ExecuteAsAdmin(std::string("DROP ROLE IF EXISTS ") + ESCAPE_USER);
        ExecuteAsAdmin(std::string("CREATE ROLE ") + ESCAPE_USER + " LOGIN PASSWORD '" + ESCAPE_PASSWORD + "'");
    }

    void TearDown() override {
        // Disconnect the test connection before dropping the role it may be logged in as
        BaseConnectionTest::TearDown();
#ifdef _WIN32
        sql_connect_dsn_values_.reset();
#endif
        if (!IsSkipped()) {
            ExecuteAsAdmin(std::string("DROP ROLE IF EXISTS ") + ESCAPE_USER);
        }
    }

    // SQLConnect takes no database argument, the database must come from the DSN. The DSNs the Windows CI
    // creates do not set one, so set it for the duration of the test.
    void SetDsnDatabaseForSqlConnect() {
#ifdef _WIN32
        sql_connect_dsn_values_ = std::make_unique<UserDsnValues>(test_dsn);
        if (sql_connect_dsn_values_->Exists()) {
            sql_connect_dsn_values_->Set("DATABASE", test_db);
        }
#endif
    }

    std::string DsnOnlyConnStr() const {
        return ConnectionStringBuilder(test_dsn, test_server, test_port)
            .withDatabase(test_db)
            .getString();
    }

    std::string ConnStr(const std::string& uid, const std::string& pwd) const {
        return ConnectionStringBuilder(test_dsn, test_server, test_port)
            .withUID(uid)
            .withPWD(pwd)
            .withDatabase(test_db)
            .getString();
    }

    void ExecuteAsAdmin(const std::string& query) const {
        SQLHENV admin_env = SQL_NULL_HENV;
        SQLHDBC admin_dbc = SQL_NULL_HDBC;
        SQLHSTMT admin_stmt = SQL_NULL_HSTMT;
        ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &admin_env));
        ASSERT_EQ(SQL_SUCCESS, SQLSetEnvAttr(admin_env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
        ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, admin_env, &admin_dbc));

        const bool connected = SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(admin_dbc, ConnStr(test_uid, test_pwd)));
        if (connected) {
            SQLAllocHandle(SQL_HANDLE_STMT, admin_dbc, &admin_stmt);
            EXPECT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(admin_stmt, query))) << query;
        }
        ODBC_HELPER::CleanUpHandles(admin_env, admin_dbc, admin_stmt);
        ASSERT_TRUE(connected) << "Admin connection failed, check TEST_USERNAME and TEST_PASSWORD";
    }

    static std::string QueryString(SQLHDBC hdbc, const std::string& query) {
        SQLHSTMT stmt = SQL_NULL_HSTMT;
        SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt);

        std::string result;
        if (SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(stmt, query)) && SQL_SUCCEEDED(SQLFetch(stmt))) {
            char buffer[256] = {0};
            SQLLEN len = 0;
            if (SQL_SUCCEEDED(SQLGetData(stmt, 1, SQL_C_CHAR, buffer, sizeof(buffer), &len))) {
                result = buffer;
            }
        }
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return result;
    }

#ifdef _WIN32
    std::unique_ptr<UserDsnValues> sql_connect_dsn_values_;
#endif
};

// A password the application encloses in braces reaches psqlODBC enclosed, so it is used literally
TEST_F(ConnectionStringEscapeIntegrationTest, DriverConnectBracedPasswordIsPassedLiterally) {
    ASSERT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(dbc, ConnStr(ESCAPE_USER, std::string("{") + ESCAPE_PASSWORD + "}"))));
    EXPECT_EQ(ESCAPE_USER, QueryString(dbc, "SELECT current_user"));
}

// A password the application does not enclose in braces is URL-decoded by psqlODBC, so its encoded form logs in
TEST_F(ConnectionStringEscapeIntegrationTest, DriverConnectUnbracedPasswordIsUrlDecoded) {
    ASSERT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(dbc, ConnStr(ESCAPE_USER, ESCAPE_PASSWORD_URL_ENCODED))));
    EXPECT_EQ(ESCAPE_USER, QueryString(dbc, "SELECT current_user"));
}

// The SQLConnect password is used literally
TEST_F(ConnectionStringEscapeIntegrationTest, SQLConnectPasswordIsPassedLiterally) {
    SetDsnDatabaseForSqlConnect();
    ASSERT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::DsnConnect(dbc, test_dsn, ESCAPE_USER, ESCAPE_PASSWORD)));
    EXPECT_EQ(ESCAPE_USER, QueryString(dbc, "SELECT current_user"));
}

// psqlODBC also URL-decodes ConnSettings, a ConnSettings the application encloses in braces runs as written
TEST_F(ConnectionStringEscapeIntegrationTest, DriverConnectBracedConnSettingsIsPassedLiterally) {
    const std::string app_name("odbc+escape%41");
    const std::string conn_str = ConnStr(test_uid, test_pwd) + "CONNSETTINGS={SET application_name TO '" + app_name + "'};";

    ASSERT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(dbc, conn_str)));
    EXPECT_EQ(app_name, QueryString(dbc, "SELECT current_setting('application_name')"));
}

// A braced value must be closed, and only followed by ';'. psqlODBC rejects such a string as well.
// The Windows driver manager rejects it before the wrapper with 01S00, the wrapper itself with 08001.
TEST_F(ConnectionStringEscapeIntegrationTest, DriverConnectMalformedBracedValueIsRejected) {
    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = {0};
    STRING_HELPER::AnsiToUnicode(ConnStr(ESCAPE_USER, std::string("{") + ESCAPE_PASSWORD + "}x").c_str(), conn_str_in);
    ASSERT_EQ(SQL_ERROR, SQLDriverConnect(dbc, nullptr, conn_str_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));

    SQLTCHAR sql_state[6] = {0};
    SQLTCHAR message[SQL_MAX_MESSAGE_LENGTH] = {0};
    SQLINTEGER native_error = 0;
    SQLSMALLINT message_len = 0;
    ASSERT_TRUE(SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 1, sql_state, &native_error, message, SQL_MAX_MESSAGE_LENGTH, &message_len)));
    const std::string state = STRING_HELPER::SqltcharToAnsi(sql_state);
    EXPECT_TRUE(state == "08001" || state == "01S00") << state << ": " << STRING_HELPER::SqltcharToAnsi(message);
}

// The output connection string keeps the braces the application wrote, and none of the internal attributes
TEST_F(ConnectionStringEscapeIntegrationTest, OutputConnectionStringKeepsBracedPassword) {
    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = {0};
    STRING_HELPER::AnsiToUnicode(ConnStr(ESCAPE_USER, std::string("{") + ESCAPE_PASSWORD + "}").c_str(), conn_str_in);
    SQLTCHAR conn_str_out[STRING_HELPER::MAX_SQLCHAR] = {0};
    SQLSMALLINT out_len = 0;
    ASSERT_TRUE(SQL_SUCCEEDED(SQLDriverConnect(dbc, nullptr, conn_str_in, SQL_NTS, conn_str_out, STRING_HELPER::MAX_SQLCHAR, &out_len, SQL_DRIVER_NOPROMPT)));

    const std::string out_str = STRING_HELPER::SqltcharToAnsi(conn_str_out);
    EXPECT_NE(std::string::npos, out_str.find(std::string("PWD={") + ESCAPE_PASSWORD + "};")) << out_str;
    // The marker of the braced values is internal
    EXPECT_EQ(std::string::npos, out_str.find("7C1D9F0E-")) << out_str;
}

// After SQLConnect, whose password is always braced, the same handle connects with an unbraced password
// that psqlODBC decodes, as on a new handle
TEST_F(ConnectionStringEscapeIntegrationTest, ReusedHandleUsesNewUnbracedPassword) {
    SetDsnDatabaseForSqlConnect();
    ASSERT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::DsnConnect(dbc, test_dsn, ESCAPE_USER, ESCAPE_PASSWORD)));
    ASSERT_TRUE(SQL_SUCCEEDED(SQLDisconnect(dbc)));

    ASSERT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(dbc, ConnStr(ESCAPE_USER, ESCAPE_PASSWORD_URL_ENCODED))));
    EXPECT_EQ(ESCAPE_USER, QueryString(dbc, "SELECT current_user"));
}

#ifdef _WIN32
// BASE_CONN holds a connection string, a braced password in it is passed literally like in a connection string
TEST_F(ConnectionStringEscapeIntegrationTest, DsnBaseConnBracedPasswordIsPassedLiterally) {
    UserDsnValues dsn_values(test_dsn);
    if (!dsn_values.Exists()) {
        GTEST_SKIP() << "TEST_DSN '" << test_dsn << "' is not a user DSN";
    }
    dsn_values.Set("UID", ESCAPE_USER);
    dsn_values.Set("BASE_CONN", std::string("PWD={") + ESCAPE_PASSWORD + "}");

    ASSERT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(dbc, DsnOnlyConnStr())));
    EXPECT_EQ(ESCAPE_USER, QueryString(dbc, "SELECT current_user"));
}
#endif
