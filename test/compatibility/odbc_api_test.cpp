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
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>

#ifdef WIN32
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>

#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

#define MAX_SQL_STATE_LEN 6
#define MAX_SQL_MESSAGE_LEN 256
#define MAX_COLUMN_NAME_LEN 256
#define MAX_BUFFER_LEN 1024
#define OID_COLUMN 23

static std::string test_server;
static std::string test_dsn;
static int test_port;
static std::string test_db;
static std::string test_uid;
static std::string test_pwd;
static std::string test_base_driver;
static std::string test_base_dsn;

static SQLTCHAR* catalog;
static SQLTCHAR* schema;
static SQLTCHAR* wild_star;
static SQLTCHAR* table;

class ODBC_API_TEST : public testing::TestWithParam<std::string> {
protected:
    SQLHENV env;
    SQLHDBC dbc;
    SQLRETURN ret;

    static void SetUpTestSuite() {
        catalog = new SQLTCHAR[MAX_BUFFER_LEN];
        STRING_HELPER::AnsiToUnicode("public", catalog);
        schema = new SQLTCHAR[MAX_BUFFER_LEN];
        STRING_HELPER::AnsiToUnicode("test_metadata", schema);
        wild_star = new SQLTCHAR[MAX_BUFFER_LEN];
        STRING_HELPER::AnsiToUnicode("%", wild_star);
        table = new SQLTCHAR[MAX_BUFFER_LEN];
        STRING_HELPER::AnsiToUnicode("TABLE", table);
    }

    static void TearDownTestSuite() {
        if (catalog) {
            delete catalog;
        }
        if (schema) {
            delete schema;
        }
        if (wild_star) {
            delete wild_star;
        }
        if (table) {
            delete table;
        }
    }

    void SetUp() override {
        std::string test_dsn = GetParam();
        EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env));
        EXPECT_EQ(SQL_SUCCESS, SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
        EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc));

        std::string conn_str = ConnectionStringBuilder(test_dsn.c_str(), test_server, test_port)
            .withDatabase(test_db)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withBaseDriver(test_base_driver)
            .withBaseDSN(test_base_dsn)
            .getString();
        EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::DriverConnect(dbc, conn_str));
    }

    void TearDown() override {
        ODBC_HELPER::CleanUpHandles(env, dbc, SQL_NULL_HSTMT);
    }
};

auto GetErrorMessage = [](SQLSMALLINT handle_type, SQLHANDLE handle, SQLRETURN original_ret = SQL_ERROR) -> std::string {
    SQLTCHAR sql_state[MAX_SQL_STATE_LEN], message[MAX_SQL_MESSAGE_LEN];
    SQLINTEGER native_error;
    SQLSMALLINT msg_len;
    SQLRETURN diag_ret = SQLGetDiagRec(handle_type, handle, 1, sql_state, &native_error, message, sizeof(message), &msg_len);
    if (SQL_SUCCESS == diag_ret) {
        return std::string("Error: ") + STRING_HELPER::SqltcharToAnsi(sql_state) + " - " + STRING_HELPER::SqltcharToAnsi(message);
    }
    return std::string("Unknown error - Original return code: ") + std::to_string(original_ret) + ", SQLGetDiagRec return code: " + std::to_string(diag_ret) + " (no diagnostic information available)";
};

auto CreateResultsFile = [](const std::string& test_dsn, const std::string& test_name) -> std::ofstream {
    std::filesystem::create_directories(test_dsn);
    std::string file_name = test_dsn + "/" + test_name + ".json";
    return std::ofstream(file_name);
};

auto FetchResults = [](SQLHSTMT stmt, std::ofstream& out_file, const std::string& func_name, SQLRETURN ret, bool first_result = false) {
    // Output the results in JSON format
    if (!first_result) out_file << ",\n";
    out_file << "  \"" << func_name << "\": {\n";
    out_file << "    \"return_code\": " << ret << ",\n";
    if (ret == SQL_SUCCESS) {
        SQLSMALLINT num_cols;
        SQLNumResultCols(stmt, &num_cols);
        out_file << "    \"rows\": [\n";
        int row_count = 0;
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            if (row_count > 0) out_file << ",\n";
            out_file << "      [";
            for (SQLSMALLINT i = 1; i <= num_cols; i++) {
                if (i > 1) out_file << ", ";
                SQLTCHAR data[MAX_SQL_MESSAGE_LEN] = {0};
                SQLLEN indicator = 0;
                SQLRETURN get_ret = SQLGetData(stmt, i, SQL_C_TCHAR, data, sizeof(data) - 1, &indicator);
                if (get_ret == SQL_SUCCESS || get_ret == SQL_SUCCESS_WITH_INFO) {
                    if (indicator == SQL_NULL_DATA) {
                        out_file << "null";
                    } else {
                        data[sizeof(data) - 1] = '\0';
                        out_file << "\"" << reinterpret_cast<char*>(data) << "\"";
                    }
                } else {
                    out_file << "null";
                }
            }
            out_file << "]";
            row_count++;
        }
        out_file << "\n    ],\n";
        out_file << "    \"row_count\": " << row_count << "\n";
    } else {
        out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
    }
    out_file << "  }";
};

TEST_P(ODBC_API_TEST, MetadataFunctionsTest) {
    std::string test_dsn = GetParam();
    SQLHSTMT stmt;

    std::ofstream out_file = CreateResultsFile(test_dsn, "MetadataFunctionsTest");

    out_file << "{\n";

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    EXPECT_EQ(ret, SQL_SUCCESS);
    ret = ODBC_HELPER::ExecuteQuery(stmt, "DROP TABLE IF EXISTS test_metadata");
    SQLCloseCursor(stmt);

    ret = ODBC_HELPER::ExecuteQuery(stmt,
        "CREATE TABLE test_metadata ("
        "id SERIAL PRIMARY KEY, "
        "name VARCHAR(50) NOT NULL, "
        "age INTEGER, "
        "salary DECIMAL(10,2), "
        "created_date DATE, "
        "is_active BOOLEAN, "
        "UNIQUE(name)"
        ")");
    SQLCloseCursor(stmt);

    // Insert test data
    ret = ODBC_HELPER::ExecuteQuery(stmt,
        "INSERT INTO test_metadata (name, age, salary, created_date, is_active) VALUES "
        "('John Doe', 30, 50000.00, '2023-01-15', true), "
        "('Jane Smith', 25, 45000.50, '2023-02-20', false)");
    SQLCloseCursor(stmt);

    ret = SQLGetTypeInfo(stmt, SQL_ALL_TYPES);
    FetchResults(stmt, out_file, "SQLGetTypeInfo", ret, true);
    SQLCloseCursor(stmt);

    ret = SQLTables(stmt, NULL, 0, catalog, SQL_NTS, wild_star, SQL_NTS, table, SQL_NTS);
    FetchResults(stmt, out_file, "SQLTables", ret);
    SQLCloseCursor(stmt);

    ret = SQLProcedures(stmt, NULL, 0, catalog, SQL_NTS, wild_star, SQL_NTS);
    FetchResults(stmt, out_file, "SQLProcedures", ret);
    SQLCloseCursor(stmt);

    ret = SQLProcedureColumns(stmt, NULL, 0, catalog, SQL_NTS, wild_star, SQL_NTS, wild_star, SQL_NTS);
    FetchResults(stmt, out_file, "SQLProcedureColumns", ret);
    SQLCloseCursor(stmt);

    ret = SQLTablePrivileges(stmt, NULL, 0, catalog, SQL_NTS, schema, SQL_NTS);
    FetchResults(stmt, out_file, "SQLTablePrivileges", ret);
    SQLCloseCursor(stmt);

    ret = SQLColumnPrivileges(stmt, NULL, 0, catalog, SQL_NTS, schema, SQL_NTS, wild_star, SQL_NTS);
    FetchResults(stmt, out_file, "SQLColumnPrivileges", ret);
    SQLCloseCursor(stmt);

    ret = SQLPrimaryKeys(stmt, NULL, 0, catalog, SQL_NTS, schema, SQL_NTS);
    FetchResults(stmt, out_file, "SQLPrimaryKeys", ret);
    SQLCloseCursor(stmt);

    ret = SQLForeignKeys(stmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0, catalog, SQL_NTS, schema, SQL_NTS);
    FetchResults(stmt, out_file, "SQLForeignKeys", ret);
    SQLCloseCursor(stmt);

    ret = SQLSpecialColumns(stmt, SQL_BEST_ROWID, NULL, 0, catalog, SQL_NTS, schema, SQL_NTS, SQL_SCOPE_CURROW, SQL_NULLABLE);
    FetchResults(stmt, out_file, "SQLSpecialColumns", ret);
    SQLCloseCursor(stmt);

    ret = SQLStatistics(stmt, NULL, 0, catalog, SQL_NTS, schema, SQL_NTS, SQL_INDEX_ALL, SQL_QUICK);
    FetchResults(stmt, out_file, "SQLStatistics", ret);
    SQLCloseCursor(stmt);

    // Additional metadata functions in JSON format
    ret = ODBC_HELPER::ExecuteQuery(stmt, "SELECT 1");
    if (ret == SQL_SUCCESS) {
        SQLSMALLINT num_cols;
        ret = SQLNumResultCols(stmt, &num_cols);
        out_file << ",\n  \"SQLNumResultCols\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"column_count\": " << num_cols << "\n";
        out_file << "  }";

        if (num_cols > 0) {
            SQLTCHAR col_name[MAX_COLUMN_NAME_LEN];
            SQLSMALLINT name_len, data_type, decimal_digits, nullable;
            SQLULEN column_size;
            SQLLEN attr_value;

            ret = SQLDescribeCol(stmt, 1, col_name, sizeof(col_name), &name_len, &data_type, &column_size, &decimal_digits, &nullable);
            out_file << ",\n  \"SQLDescribeCol\": {\n";
            out_file << "    \"return_code\": " << ret << ",\n";
            out_file << "    \"column_name\": \"" << STRING_HELPER::SqltcharToAnsi(col_name) << "\",\n";
            out_file << "    \"data_type\": " << data_type << "\n";
            out_file << "  }";

            ret = SQLColAttribute(stmt, 1, SQL_DESC_TYPE, NULL, 0, NULL, &attr_value);
            out_file << ",\n  \"SQLColAttribute\": {\n";
            out_file << "    \"return_code\": " << ret << ",\n";
            out_file << "    \"attribute_type\": " << attr_value << "\n";
            out_file << "  }";

            ret = SQLColAttributes(stmt, 1, SQL_COLUMN_TYPE, NULL, 0, NULL, &attr_value);
            out_file << ",\n  \"SQLColAttributes\": {\n";
            out_file << "    \"return_code\": " << ret << ",\n";
            out_file << "    \"column_type\": " << attr_value << "\n";
            out_file << "  }";
        }
    }
    SQLCloseCursor(stmt);

    SQLTCHAR prepare_stmt[MAX_BUFFER_LEN] = { 0 };
    STRING_HELPER::AnsiToUnicode("SELECT ? as int_param, ? as varchar_param, ? as date_param, ? as decimal_param, ? as bool_param", prepare_stmt);
    ret = SQLPrepare(stmt, prepare_stmt, SQL_NTS);
    if (ret == SQL_SUCCESS) {
        SQLSMALLINT num_params;
        ret = SQLNumParams(stmt, &num_params);
        out_file << ",\n  \"SQLNumParams\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"parameter_count\": " << num_params << "\n";
        out_file << "  }";
    }

    out_file << "\n}\n";

    ret = ODBC_HELPER::ExecuteQuery(stmt, "DROP TABLE IF EXISTS test_metadata");
    SQLCloseCursor(stmt);

    out_file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

TEST_P(ODBC_API_TEST, SQLGetInfoTest) {
    std::string test_dsn = GetParam();

    std::ofstream out_file = CreateResultsFile(test_dsn, "SQLGetInfoTest");
    out_file << "{\n";

    std::map<SQLUSMALLINT, std::pair<std::string, bool /* is numeric attribute */>> info_attrs = {
        // Intentionally skipping the comparison for driver name datasource name and driver odbc ver
        {SQL_DBMS_NAME, {"SQL_DBMS_NAME", false}},
        {SQL_DBMS_VER, {"SQL_DBMS_VER", false}},
        {SQL_DATABASE_NAME, {"SQL_DATABASE_NAME", false}},
        {SQL_SERVER_NAME, {"SQL_SERVER_NAME", false}},
        {SQL_USER_NAME, {"SQL_USER_NAME", false}},
        {SQL_ACTIVE_CONNECTIONS, {"SQL_ACTIVE_CONNECTIONS", true}},
        {SQL_SQL_CONFORMANCE, {"SQL_SQL_CONFORMANCE", true}},
        {SQL_ODBC_INTERFACE_CONFORMANCE, {"SQL_ODBC_INTERFACE_CONFORMANCE", true}},
        {SQL_STANDARD_CLI_CONFORMANCE, {"SQL_STANDARD_CLI_CONFORMANCE", true}},
        {SQL_TXN_CAPABLE, {"SQL_TXN_CAPABLE", true}},
        {SQL_TXN_ISOLATION_OPTION, {"SQL_TXN_ISOLATION_OPTION", true}},
        {SQL_DEFAULT_TXN_ISOLATION, {"SQL_DEFAULT_TXN_ISOLATION", true}},
        {SQL_MAX_IDENTIFIER_LEN, {"SQL_MAX_IDENTIFIER_LEN", true}},
        {SQL_MAX_TABLE_NAME_LEN, {"SQL_MAX_TABLE_NAME_LEN", true}},
        {SQL_MAX_COLUMN_NAME_LEN, {"SQL_MAX_COLUMN_NAME_LEN", true}},
        {SQL_MAX_CURSOR_NAME_LEN, {"SQL_MAX_CURSOR_NAME_LEN", true}},
        {SQL_MAX_SCHEMA_NAME_LEN, {"SQL_MAX_SCHEMA_NAME_LEN", true}},
        {SQL_MAX_CATALOG_NAME_LEN, {"SQL_MAX_CATALOG_NAME_LEN", true}},
        {SQL_MAX_USER_NAME_LEN, {"SQL_MAX_USER_NAME_LEN", true}},
        {SQL_MAX_PROCEDURE_NAME_LEN, {"SQL_MAX_PROCEDURE_NAME_LEN", true}},
        {SQL_NUMERIC_FUNCTIONS, {"SQL_NUMERIC_FUNCTIONS", true}},
        {SQL_STRING_FUNCTIONS, {"SQL_STRING_FUNCTIONS", true}},
        {SQL_TIMEDATE_FUNCTIONS, {"SQL_TIMEDATE_FUNCTIONS", true}},
        {SQL_SYSTEM_FUNCTIONS, {"SQL_SYSTEM_FUNCTIONS", true}},
        {SQL_CONVERT_FUNCTIONS, {"SQL_CONVERT_FUNCTIONS", true}},
        {SQL_CURSOR_COMMIT_BEHAVIOR, {"SQL_CURSOR_COMMIT_BEHAVIOR", true}},
        {SQL_CURSOR_ROLLBACK_BEHAVIOR, {"SQL_CURSOR_ROLLBACK_BEHAVIOR", true}},
        {SQL_SCROLL_OPTIONS, {"SQL_SCROLL_OPTIONS", true}},
        {SQL_STATIC_CURSOR_ATTRIBUTES1, {"SQL_STATIC_CURSOR_ATTRIBUTES1", true}},
        {SQL_STATIC_CURSOR_ATTRIBUTES2, {"SQL_STATIC_CURSOR_ATTRIBUTES2", true}},
        {SQL_DYNAMIC_CURSOR_ATTRIBUTES1, {"SQL_DYNAMIC_CURSOR_ATTRIBUTES1", true}},
        {SQL_DYNAMIC_CURSOR_ATTRIBUTES2, {"SQL_DYNAMIC_CURSOR_ATTRIBUTES2", true}},
        {SQL_CATALOG_TERM, {"SQL_CATALOG_TERM", false}},
        {SQL_SCHEMA_TERM, {"SQL_SCHEMA_TERM", false}},
        {SQL_TABLE_TERM, {"SQL_TABLE_TERM", false}},
        {SQL_PROCEDURE_TERM, {"SQL_PROCEDURE_TERM", false}},
        {SQL_CONVERT_BIGINT, {"SQL_CONVERT_BIGINT", true}},
        {SQL_CONVERT_INTEGER, {"SQL_CONVERT_INTEGER", true}},
        {SQL_CONVERT_SMALLINT, {"SQL_CONVERT_SMALLINT", true}},
        {SQL_CONVERT_VARCHAR, {"SQL_CONVERT_VARCHAR", true}},
        {SQL_CONVERT_CHAR, {"SQL_CONVERT_CHAR", true}},
        {SQL_CONVERT_DECIMAL, {"SQL_CONVERT_DECIMAL", true}},
        {SQL_CONVERT_NUMERIC, {"SQL_CONVERT_NUMERIC", true}},
        {SQL_CONVERT_REAL, {"SQL_CONVERT_REAL", true}},
        {SQL_CONVERT_FLOAT, {"SQL_CONVERT_FLOAT", true}},
        {SQL_CONVERT_DOUBLE, {"SQL_CONVERT_DOUBLE", true}},
        {SQL_CONVERT_DATE, {"SQL_CONVERT_DATE", true}},
        {SQL_CONVERT_TIME, {"SQL_CONVERT_TIME", true}},
        {SQL_CONVERT_TIMESTAMP, {"SQL_CONVERT_TIMESTAMP", true}},
        {SQL_CONVERT_BINARY, {"SQL_CONVERT_BINARY", true}},
        {SQL_CONVERT_VARBINARY, {"SQL_CONVERT_VARBINARY", true}},
        {SQL_CONVERT_BIT, {"SQL_CONVERT_BIT", true}},
        {SQL_DATETIME_LITERALS, {"SQL_DATETIME_LITERALS", true}},
        {SQL_KEYWORDS, {"SQL_KEYWORDS", false}}
    };

    bool first_attr = true;
    for (const auto &attr: info_attrs) {
        if (!first_attr) out_file << ",\n";
        first_attr = false;

        SQLTCHAR buffer[MAX_BUFFER_LEN] = {};
        SQLSMALLINT str_len = 0;
        ret = SQLGetInfo(dbc, attr.first, buffer, sizeof(buffer), &str_len);

        out_file << "  \"" << attr.second.first << "\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"attribute_id\": " << attr.first;

        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            out_file << "\n  }";
            continue;
        }

        if (!attr.second.second) {
            // Is a string attribute.
            out_file << ",\n    \"value\": \"" << STRING_HELPER::SqltcharToAnsi(buffer) << "\"\n";
        } else {
            // Is a numeric attribute.
            out_file << ",\n    \"value\": ";
            if (str_len == sizeof(SQLUSMALLINT)) {
                out_file << *reinterpret_cast<SQLUSMALLINT *>(buffer);
            } else if (str_len == sizeof(SQLUINTEGER)) {
                out_file << *reinterpret_cast<SQLUINTEGER *>(buffer);
            } else {
                out_file << "null,\n    \"note\": \"numeric, len=" << str_len << "\"";
            }
            out_file << "\n";
        }
        out_file << "  }";
    }

    out_file << "\n}\n";
    out_file.close();
}

TEST_P(ODBC_API_TEST, SQLColumnsTest) {
    std::string test_dsn = GetParam();
    SQLHSTMT stmt;

    std::ofstream out_file = CreateResultsFile(test_dsn, "SQLColumnsTest");
    out_file << "{\n";

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    EXPECT_EQ(ret, SQL_SUCCESS);

    ret = SQLColumns(stmt, NULL, 0, catalog, SQL_NTS, wild_star, SQL_NTS, wild_star, SQL_NTS);
    out_file << "  \"SQLColumns\": {\n";
    out_file << "    \"return_code\": " << ret;

    if (ret == SQL_SUCCESS) {
        SQLSMALLINT num_cols;
        SQLNumResultCols(stmt, &num_cols);

        out_file << ",\n    \"column_names\": [";
        bool first_col = true;
        for (SQLSMALLINT i = 1; i <= num_cols; i++) {
            if (i == OID_COLUMN) continue; // Intentionally skipping the OID column, as the ID changes.
            if (!first_col) out_file << ", ";
            first_col = false;
            SQLTCHAR col_name[MAX_COLUMN_NAME_LEN];
            SQLSMALLINT name_len;
            SQLDescribeCol(stmt, i, col_name, sizeof(col_name), &name_len, NULL, NULL, NULL, NULL);
            out_file << "\"" << STRING_HELPER::SqltcharToAnsi(col_name) << "\"";
        }
        out_file << "],\n";

        out_file << "    \"rows\": [\n";
        int row_count = 0;
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            if (row_count > 0) out_file << ",\n";
            out_file << "      [";
            bool first_data = true;
            for (SQLSMALLINT i = 1; i <= num_cols; i++) {
                if (i == OID_COLUMN) continue; // Intentionally skipping the OID column, as the ID changes.
                if (!first_data) out_file << ", ";
                first_data = false;
                SQLTCHAR data[MAX_SQL_MESSAGE_LEN] = {};
                SQLLEN indicator = 0;
                SQLRETURN get_ret = SQLGetData(stmt, i, SQL_C_TCHAR, data, sizeof(data) - 1, &indicator);
                if (get_ret == SQL_SUCCESS || get_ret == SQL_SUCCESS_WITH_INFO) {
                    if (indicator == SQL_NULL_DATA) {
                        out_file << "null";
                    } else {
                        data[sizeof(data) - 1] = '\0';
                        out_file << "\"" << STRING_HELPER::SqltcharToAnsi(data) << "\"";
                    }
                } else {
                    out_file << "null";
                }
            }
            out_file << "]";
            row_count++;
        }
        out_file << "\n    ],\n";
        out_file << "    \"row_count\": " << row_count << "\n";
    } else {
        out_file << ",\n    \"error\": \"" << GetErrorMessage(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
    }
    out_file << "  }\n";
    SQLCloseCursor(stmt);

    out_file << "}\n";
    out_file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

TEST_P(ODBC_API_TEST, ConnectionAttributesTest) {
    std::string test_dsn = GetParam();
    std::ofstream out_file = CreateResultsFile(test_dsn, "ConnectionAttributesTest");
    out_file << "{\n";

    // SQLGetConnectAttr - string attribute (SQL_ATTR_CURRENT_CATALOG)
    {
        SQLTCHAR buffer[MAX_BUFFER_LEN] = {0};
        SQLINTEGER str_len = 0;
        ret = SQLGetConnectAttr(dbc, SQL_ATTR_CURRENT_CATALOG, buffer, sizeof(buffer), &str_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << "  \"SQLGetConnectAttr_CURRENT_CATALOG\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"value\": \"" << STRING_HELPER::SqltcharToAnsi(buffer) << "\",\n";
            out_file << "    \"string_length\": " << str_len << "\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_DBC, dbc, ret) << "\"\n";
        }
        out_file << "  }";
    }

    // SQLGetConnectAttr - numeric attribute (SQL_ATTR_AUTOCOMMIT)
    {
        SQLUINTEGER value = 0;
        SQLINTEGER str_len = 0;
        ret = SQLGetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, &value, sizeof(value), &str_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetConnectAttr_AUTOCOMMIT\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"value\": " << value << "\n";
        out_file << "  }";
    }

    // SQLSetConnectAttr
    {
        ret = SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
            reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), SQL_IS_UINTEGER);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLSetConnectAttr_AUTOCOMMIT_OFF\": {\n";
        out_file << "    \"return_code\": " << ret << "\n";
        out_file << "  }";
        // Reset
        SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
            reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), SQL_IS_UINTEGER);
    }

    // SQLGetConnectOption (deprecated variant)
    {
        SQLTCHAR buffer[MAX_BUFFER_LEN] = {0};
        ret = SQLGetConnectOption(dbc, SQL_ATTR_CURRENT_CATALOG, buffer);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetConnectOption_CURRENT_CATALOG\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"value\": \"" << STRING_HELPER::SqltcharToAnsi(buffer) << "\"\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_DBC, dbc, ret) << "\"\n";
        }
        out_file << "  }";
    }

    out_file << "\n}\n";
    out_file.close();
}

TEST_P(ODBC_API_TEST, StatementAttributesTest) {
    std::string test_dsn = GetParam();
    SQLHSTMT stmt;
    std::ofstream out_file = CreateResultsFile(test_dsn, "StatementAttributesTest");
    out_file << "{\n";

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    EXPECT_EQ(ret, SQL_SUCCESS);

    // SQLSetStmtAttr
    {
        ret = SQLSetStmtAttr(stmt, SQL_ATTR_QUERY_TIMEOUT,
            reinterpret_cast<SQLPOINTER>(30), SQL_IS_UINTEGER);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << "  \"SQLSetStmtAttr_QUERY_TIMEOUT\": {\n";
        out_file << "    \"return_code\": " << ret << "\n";
        out_file << "  }";
    }

    // SQLGetStmtAttr
    {
        SQLULEN value = 0;
        SQLINTEGER str_len = 0;
        ret = SQLGetStmtAttr(stmt, SQL_ATTR_QUERY_TIMEOUT, &value, sizeof(value), &str_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetStmtAttr_QUERY_TIMEOUT\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"value\": " << value << "\n";
        out_file << "  }";
    }

    {
        SQLULEN value = 0;
        SQLINTEGER str_len = 0;
        ret = SQLGetStmtAttr(stmt, SQL_ATTR_CURSOR_TYPE, &value, sizeof(value), &str_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetStmtAttr_CURSOR_TYPE\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"value\": " << value << "\n";
        out_file << "  }";
    }

    out_file << "\n}\n";
    out_file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

TEST_P(ODBC_API_TEST, CursorNameTest) {
    std::string test_dsn = GetParam();
    SQLHSTMT stmt;
    std::ofstream out_file = CreateResultsFile(test_dsn, "CursorNameTest");
    out_file << "{\n";

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    EXPECT_EQ(ret, SQL_SUCCESS);

    // SQLSetCursorName
    {
        SQLTCHAR cursor_name[MAX_BUFFER_LEN] = {0};
        STRING_HELPER::AnsiToUnicode("test_cursor", cursor_name);
        ret = SQLSetCursorName(stmt, cursor_name, SQL_NTS);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << "  \"SQLSetCursorName\": {\n";
        out_file << "    \"return_code\": " << ret << "\n";
        out_file << "  }";
    }

    // SQLGetCursorName
    {
        SQLTCHAR cursor_out[MAX_BUFFER_LEN] = {0};
        SQLSMALLINT name_len = 0;
        ret = SQLGetCursorName(stmt, cursor_out, MAX_BUFFER_LEN, &name_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetCursorName\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"cursor_name\": \"" << STRING_HELPER::SqltcharToAnsi(cursor_out) << "\",\n";
            out_file << "    \"name_length\": " << name_len << "\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
        }
        out_file << "  }";
    }

    out_file << "\n}\n";
    out_file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

TEST_P(ODBC_API_TEST, DiagnosticsFunctionsTest) {
    std::string test_dsn = GetParam();
    SQLHSTMT stmt;
    std::ofstream out_file = CreateResultsFile(test_dsn, "DiagnosticsFunctionsTest");
    out_file << "{\n";

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    EXPECT_EQ(ret, SQL_SUCCESS);

    // Trigger an error
    ret = ODBC_HELPER::ExecuteQuery(stmt, "SELECT * FROM nonexistent_table_diag_test");
    EXPECT_NE(ret, SQL_SUCCESS);

    // SQLGetDiagRec
    {
        SQLTCHAR sqlstate[MAX_SQL_STATE_LEN] = {0};
        SQLINTEGER native_error = 0;
        SQLTCHAR message[MAX_SQL_MESSAGE_LEN] = {0};
        SQLSMALLINT msg_len = 0;

        ret = SQLGetDiagRec(SQL_HANDLE_STMT, stmt, 1, sqlstate, &native_error,
            message, MAX_SQL_MESSAGE_LEN, &msg_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << "  \"SQLGetDiagRec\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"sqlstate\": \"" << STRING_HELPER::SqltcharToAnsi(sqlstate) << "\",\n";
            out_file << "    \"native_error\": " << native_error << ",\n";
            out_file << "    \"message_length\": " << msg_len << "\n";
        } else {
            out_file << "    \"error\": \"no diagnostic record available\"\n";
        }
        out_file << "  }";
    }

    // SQLGetDiagField - string field (SQL_DIAG_SQLSTATE)
    {
        SQLTCHAR diag_info[MAX_BUFFER_LEN] = {0};
        SQLSMALLINT str_len = 0;
        ret = SQLGetDiagField(SQL_HANDLE_STMT, stmt, 1, SQL_DIAG_SQLSTATE,
            diag_info, sizeof(diag_info), &str_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetDiagField_SQLSTATE\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"value\": \"" << STRING_HELPER::SqltcharToAnsi(diag_info) << "\",\n";
            out_file << "    \"string_length\": " << str_len << "\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
        }
        out_file << "  }";
    }

    // SQLGetDiagField - numeric header field (SQL_DIAG_NUMBER)
    {
        SQLINTEGER diag_count = 0;
        SQLSMALLINT str_len = 0;
        ret = SQLGetDiagField(SQL_HANDLE_STMT, stmt, 0, SQL_DIAG_NUMBER,
            &diag_count, sizeof(diag_count), &str_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetDiagField_NUMBER\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"value\": " << diag_count << "\n";
        out_file << "  }";
    }

    // SQLGetDiagField - string record field (SQL_DIAG_MESSAGE_TEXT)
    {
        SQLTCHAR diag_msg[MAX_BUFFER_LEN] = {0};
        SQLSMALLINT str_len = 0;
        ret = SQLGetDiagField(SQL_HANDLE_STMT, stmt, 1, SQL_DIAG_MESSAGE_TEXT,
            diag_msg, sizeof(diag_msg), &str_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetDiagField_MESSAGE_TEXT\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"string_length\": " << str_len << "\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
        }
        out_file << "  }";
    }

    // SQLError (deprecated but implemented in the driver)
    {
        // Trigger a fresh error for SQLError
        SQLCloseCursor(stmt);
        ret = ODBC_HELPER::ExecuteQuery(stmt, "SELECT * FROM another_nonexistent_table");
        EXPECT_NE(ret, SQL_SUCCESS);

        SQLTCHAR sqlstate[MAX_SQL_STATE_LEN] = {0};
        SQLINTEGER native_error = 0;
        SQLTCHAR message[MAX_SQL_MESSAGE_LEN] = {0};
        SQLSMALLINT msg_len = 0;

        ret = SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, stmt, sqlstate, &native_error,
            message, MAX_SQL_MESSAGE_LEN, &msg_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLError\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"sqlstate\": \"" << STRING_HELPER::SqltcharToAnsi(sqlstate) << "\",\n";
            out_file << "    \"native_error\": " << native_error << ",\n";
            out_file << "    \"message_length\": " << msg_len << "\n";
        } else {
            out_file << "    \"error\": \"no error record available\"\n";
        }
        out_file << "  }";
    }

    out_file << "\n}\n";
    out_file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

TEST_P(ODBC_API_TEST, StatementExecutionTest) {
    std::string test_dsn = GetParam();
    SQLHSTMT stmt;
    std::ofstream out_file = CreateResultsFile(test_dsn, "StatementExecutionTest");
    out_file << "{\n";

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    EXPECT_EQ(ret, SQL_SUCCESS);

    // SQLExecDirect with string result + SQLGetData
    {
        SQLTCHAR query[MAX_BUFFER_LEN] = {0};
        STRING_HELPER::AnsiToUnicode("SELECT 'some_value' AS test_col", query);
        ret = SQLExecDirect(stmt, query, SQL_NTS);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << "  \"SQLExecDirect\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            SQLTCHAR data[MAX_BUFFER_LEN] = {0};
            SQLLEN indicator = 0;
            SQLFetch(stmt);
            ret = SQLGetData(stmt, 1, SQL_C_TCHAR, data, sizeof(data), &indicator);
            EXPECT_EQ(ret, SQL_SUCCESS);
            out_file << "    \"SQLGetData_return_code\": " << ret << ",\n";
            out_file << "    \"value\": \"" << STRING_HELPER::SqltcharToAnsi(data) << "\",\n";
            out_file << "    \"indicator\": " << indicator << "\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
        }
        out_file << "  }";
        SQLCloseCursor(stmt);
    }

    // SQLPrepare + SQLBindParameter + SQLExecute
    {
        SQLTCHAR prepare_query[MAX_BUFFER_LEN] = {0};
        STRING_HELPER::AnsiToUnicode("SELECT ? AS param_result", prepare_query);
        ret = SQLPrepare(stmt, prepare_query, SQL_NTS);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLPrepare\": {\n";
        out_file << "    \"return_code\": " << ret << "\n";
        out_file << "  }";

        SQLTCHAR param_value[MAX_BUFFER_LEN] = {0};
        STRING_HELPER::AnsiToUnicode("test_param_value", param_value);
        SQLLEN param_len = SQL_NTS;

        ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TCHAR, SQL_VARCHAR,
            MAX_BUFFER_LEN, 0, param_value, sizeof(param_value), &param_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLBindParameter\": {\n";
        out_file << "    \"return_code\": " << ret << "\n";
        out_file << "  }";

        ret = SQLExecute(stmt);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLExecute\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            SQLTCHAR result[MAX_BUFFER_LEN] = {0};
            SQLLEN ind = 0;
            SQLFetch(stmt);
            SQLGetData(stmt, 1, SQL_C_TCHAR, result, sizeof(result), &ind);
            out_file << "    \"value\": \"" << STRING_HELPER::SqltcharToAnsi(result) << "\"\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
        }
        out_file << "  }";
        SQLCloseCursor(stmt);
    }

    // SQLBindCol + SQLFetch
    {
        SQLTCHAR query[MAX_BUFFER_LEN] = {0};
        STRING_HELPER::AnsiToUnicode("SELECT 'bind_col_test' AS col1, 42 AS col2", query);
        ret = SQLExecDirect(stmt, query, SQL_NTS);
        EXPECT_EQ(ret, SQL_SUCCESS);

        SQLTCHAR str_result[MAX_BUFFER_LEN] = {0};
        SQLLEN str_ind = 0;
        SQLINTEGER int_result = 0;
        SQLLEN int_ind = 0;

        SQLBindCol(stmt, 1, SQL_C_TCHAR, str_result, sizeof(str_result), &str_ind);
        ret = SQLBindCol(stmt, 2, SQL_C_SLONG, &int_result, sizeof(int_result), &int_ind);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLBindCol\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";

        ret = SQLFetch(stmt);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << "    \"fetch_return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"string_value\": \"" << STRING_HELPER::SqltcharToAnsi(str_result) << "\",\n";
            out_file << "    \"int_value\": " << int_result << "\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
        }
        out_file << "  }";
        SQLCloseCursor(stmt);
    }

    // SQLPutData (data-at-execution flow)
    {
        SQLTCHAR put_query[MAX_BUFFER_LEN] = {0};
        STRING_HELPER::AnsiToUnicode("SELECT ? AS put_result", put_query);
        ret = SQLPrepare(stmt, put_query, SQL_NTS);
        EXPECT_EQ(ret, SQL_SUCCESS);

        SQLLEN data_at_exec = SQL_DATA_AT_EXEC;
        SQLPOINTER param_id = reinterpret_cast<SQLPOINTER>(1);
        ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TCHAR, SQL_VARCHAR,
            MAX_BUFFER_LEN, 0, param_id, 0, &data_at_exec);
        EXPECT_EQ(ret, SQL_SUCCESS);

        ret = SQLExecute(stmt);
        out_file << ",\n  \"SQLPutData\": {\n";
        out_file << "    \"execute_return_code\": " << ret << ",\n";

        if (ret == SQL_NEED_DATA) {
            SQLPOINTER value_ptr = nullptr;
            ret = SQLParamData(stmt, &value_ptr);
            if (ret == SQL_NEED_DATA) {
                SQLTCHAR put_value[MAX_BUFFER_LEN] = {0};
                STRING_HELPER::AnsiToUnicode("put_data_test", put_value);
                ret = SQLPutData(stmt, put_value, SQL_NTS);
                out_file << "    \"put_data_return_code\": " << ret << ",\n";

                ret = SQLParamData(stmt, &value_ptr);
                out_file << "    \"final_return_code\": " << ret << "\n";
            } else {
                out_file << "    \"param_data_return_code\": " << ret << "\n";
            }
        } else {
            out_file << "    \"note\": \"SQL_NEED_DATA not returned\"\n";
        }
        out_file << "  }";
        SQLCloseCursor(stmt);
    }

    // SQLNativeSql
    {
        SQLTCHAR sql_in[MAX_BUFFER_LEN] = {0};
        STRING_HELPER::AnsiToUnicode("SELECT {fn NOW()}", sql_in);
        SQLTCHAR sql_out[MAX_BUFFER_LEN] = {0};
        SQLINTEGER out_len = 0;

        ret = SQLNativeSql(dbc, sql_in, SQL_NTS, sql_out, MAX_BUFFER_LEN, &out_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLNativeSql\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"output\": \"" << STRING_HELPER::SqltcharToAnsi(sql_out) << "\",\n";
            out_file << "    \"output_length\": " << out_len << "\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_DBC, dbc, ret) << "\"\n";
        }
        out_file << "  }";
    }

    out_file << "\n}\n";
    out_file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

TEST_P(ODBC_API_TEST, DescriptorFunctionsTest) {
    std::string test_dsn = GetParam();
    SQLHSTMT stmt;
    std::ofstream out_file = CreateResultsFile(test_dsn, "DescriptorFunctionsTest");
    out_file << "{\n";

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    EXPECT_EQ(ret, SQL_SUCCESS);

    // Execute a query to populate the IRD (Implementation Row Descriptor)
    ret = ODBC_HELPER::ExecuteQuery(stmt, "SELECT 'desc_test' AS col1, 123 AS col2");
    EXPECT_TRUE(SQL_SUCCEEDED(ret));

    // Get the IRD handle
    SQLHDESC ird = SQL_NULL_HDESC;
    ret = SQLGetStmtAttr(stmt, SQL_ATTR_IMP_ROW_DESC, &ird, SQL_IS_POINTER, nullptr);
    EXPECT_EQ(ret, SQL_SUCCESS);

    // SQLGetDescField - get column name (string field)
    {
        SQLTCHAR field_value[MAX_BUFFER_LEN] = {0};
        SQLINTEGER str_len = 0;
        ret = SQLGetDescField(ird, 1, SQL_DESC_NAME, field_value, sizeof(field_value), &str_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << "  \"SQLGetDescField_NAME\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"value\": \"" << STRING_HELPER::SqltcharToAnsi(field_value) << "\",\n";
            out_file << "    \"string_length\": " << str_len << "\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_DESC, ird, ret) << "\"\n";
        }
        out_file << "  }";
    }

    // SQLGetDescField - get column type (numeric field)
    {
        SQLSMALLINT type_value = 0;
        SQLINTEGER str_len = 0;
        ret = SQLGetDescField(ird, 1, SQL_DESC_TYPE, &type_value, sizeof(type_value), &str_len);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetDescField_TYPE\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"value\": " << type_value << "\n";
        out_file << "  }";
    }

    // SQLGetDescRec
    {
        SQLTCHAR name[MAX_BUFFER_LEN] = {0};
        SQLSMALLINT name_len = 0, type = 0, sub_type = 0, precision = 0, scale = 0, nullable = 0;
        SQLLEN length = 0;
        ret = SQLGetDescRec(ird, 1, name, MAX_BUFFER_LEN, &name_len,
            &type, &sub_type, &length, &precision, &scale, &nullable);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLGetDescRec\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        if (SQL_SUCCEEDED(ret)) {
            out_file << "    \"name\": \"" << STRING_HELPER::SqltcharToAnsi(name) << "\",\n";
            out_file << "    \"name_length\": " << name_len << ",\n";
            out_file << "    \"type\": " << type << ",\n";
            out_file << "    \"nullable\": " << nullable << "\n";
        } else {
            out_file << "    \"error\": \"" << GetErrorMessage(SQL_HANDLE_DESC, ird, ret) << "\"\n";
        }
        out_file << "  }";
    }

    SQLCloseCursor(stmt);

    // SQLSetDescField on APD (Application Parameter Descriptor)
    {
        SQLHDESC apd = SQL_NULL_HDESC;
        SQLGetStmtAttr(stmt, SQL_ATTR_APP_PARAM_DESC, &apd, SQL_IS_POINTER, nullptr);

        ret = SQLSetDescField(apd, 1, SQL_DESC_TYPE,
            reinterpret_cast<SQLPOINTER>(SQL_C_CHAR), SQL_IS_SMALLINT);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLSetDescField\": {\n";
        out_file << "    \"return_code\": " << ret << "\n";
        out_file << "  }";
    }

    // SQLSetDescRec on ARD (Application Row Descriptor)
    {
        SQLHDESC ard = SQL_NULL_HDESC;
        SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, &ard, SQL_IS_POINTER, nullptr);

        SQLTCHAR data_buf[MAX_BUFFER_LEN] = {0};
        SQLLEN str_len = 0;
        SQLLEN indicator = 0;
        ret = SQLSetDescRec(ard, 1, SQL_C_TCHAR, 0, MAX_BUFFER_LEN, 0, 0,
            data_buf, &str_len, &indicator);
        EXPECT_EQ(ret, SQL_SUCCESS);
        out_file << ",\n  \"SQLSetDescRec\": {\n";
        out_file << "    \"return_code\": " << ret << "\n";
        out_file << "  }";
    }

    out_file << "\n}\n";
    out_file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

static std::vector<std::string> getDsnValues() {
    test_server = TEST_UTILS::GetEnvVar("TEST_SERVER", "localhost");
    std::string port_str = TEST_UTILS::GetEnvVar("TEST_PORT", "5432");
    test_port = std::strtol(port_str.c_str(), nullptr, 0);

    test_dsn = TEST_UTILS::GetEnvVar("TEST_DSN", "wrapper-dsn");

    test_db = TEST_UTILS::GetEnvVar("TEST_DATABASE");
    test_uid = TEST_UTILS::GetEnvVar("TEST_USERNAME");
    test_pwd = TEST_UTILS::GetEnvVar("TEST_PASSWORD");
    test_base_driver = TEST_UTILS::GetEnvVar("TEST_BASE_DRIVER");
    test_base_dsn = TEST_UTILS::GetEnvVar("TEST_BASE_DSN");

    return std::vector<std::string> {
        test_dsn,
        test_base_dsn
    };
}

INSTANTIATE_TEST_SUITE_P(
    MetadataFunctionsTest,
    ODBC_API_TEST,
    testing::ValuesIn(getDsnValues())
);
