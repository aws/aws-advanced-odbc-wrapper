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

#define MAX_SQL_STATE_LEN 6
#define MAX_SQL_MESSAGE_LEN 256
#define MAX_COLUMN_NAME_LEN 256
#define MAX_BUFFER_LEN 1024
#define OID_COLUMN 23

static char* test_server;
static char* test_dsn;
static int test_port;
static char* test_db;
static char* test_uid;
static char* test_pwd;
static char* test_base_driver;
static char* test_base_dsn;

class ODBC_API_TEST : public testing::TestWithParam<std::string> {
protected:
    SQLHENV env;
    SQLHDBC dbc;
    SQLRETURN ret;
    
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    
    void SetUp() override {
        std::string test_dsn = GetParam();
        
        ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
        EXPECT_EQ(ret, SQL_SUCCESS);

        ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
        EXPECT_EQ(ret, SQL_SUCCESS);

        ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
        EXPECT_EQ(ret, SQL_SUCCESS);

        ConnectionStringBuilder builder = ConnectionStringBuilder(test_dsn.c_str(), test_server, test_port);
        RDS_STR conn_str = builder.withDatabase(test_db)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withBaseDriver(test_base_driver)
            .withBaseDSN(test_base_dsn)
            .getRdsString();

        ret = SQLDriverConnect(dbc, NULL, AS_SQLTCHAR(conn_str.c_str()), SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
        EXPECT_EQ(ret, SQL_SUCCESS);
    }
    
    void TearDown() override {
        SQLDisconnect(dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, dbc);
        SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
};

auto get_error_message = [](SQLSMALLINT handle_type, SQLHANDLE handle, SQLRETURN original_ret = SQL_ERROR) -> std::string {
    SQLCHAR sql_state[MAX_SQL_STATE_LEN], message[MAX_SQL_MESSAGE_LEN];
    SQLINTEGER native_error;
    SQLSMALLINT msg_len;
    SQLRETURN diag_ret = SQLGetDiagRec(handle_type, handle, 1, sql_state, &native_error, message, sizeof(message), &msg_len);
    if (diag_ret == SQL_SUCCESS) {
        return std::string("Error: ") + reinterpret_cast<char*>(sql_state) + " - " + reinterpret_cast<char*>(message);
    }
    return std::string("Unknown error - Original return code: ") + std::to_string(original_ret) + ", SQLGetDiagRec return code: " + std::to_string(diag_ret) + " (no diagnostic information available)";
};

auto create_test_file = [](const std::string& test_dsn, const std::string& test_name) -> std::ofstream {
    std::filesystem::create_directories(test_dsn);
    std::string file_name = test_dsn + "/" + test_name + ".json";
    return std::ofstream(file_name);
};

auto fetch_results = [](SQLHSTMT stmt, std::ofstream& out_file, const std::string& func_name, SQLRETURN ret, bool first_result = false) {
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
                SQLCHAR data[MAX_SQL_MESSAGE_LEN] = {0};
                SQLLEN indicator = 0;
                SQLRETURN get_ret = SQLGetData(stmt, i, SQL_C_CHAR, data, sizeof(data) - 1, &indicator);
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
        out_file << "    \"error\": \"" << get_error_message(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
    }
    out_file << "  }";
};

TEST_P(ODBC_API_TEST, MetadataFunctionsTest) {
    std::string test_dsn = GetParam();
    SQLHSTMT stmt;

    std::ofstream out_file = create_test_file(test_dsn, "MetadataFunctionsTest");

    out_file << "{\n";

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    EXPECT_EQ(ret, SQL_SUCCESS);

    ret = SQLExecDirect(stmt, const_cast<SQLCHAR *>(reinterpret_cast<const SQLCHAR *>(
        "DROP TABLE IF EXISTS test_metadata")), SQL_NTS);
    SQLCloseCursor(stmt);
    
    ret = SQLExecDirect(stmt, const_cast<SQLCHAR *>(reinterpret_cast<const SQLCHAR *>(
        "CREATE TABLE test_metadata ("
        "id SERIAL PRIMARY KEY, "
        "name VARCHAR(50) NOT NULL, "
        "age INTEGER, "
        "salary DECIMAL(10,2), "
        "created_date DATE, "
        "is_active BOOLEAN, "
        "UNIQUE(name)"
        ")")), SQL_NTS);
    SQLCloseCursor(stmt);

    // Insert test data
    ret = SQLExecDirect(stmt, const_cast<SQLCHAR *>(reinterpret_cast<const SQLCHAR *>(
        "INSERT INTO test_metadata (name, age, salary, created_date, is_active) VALUES "
        "('John Doe', 30, 50000.00, '2023-01-15', true), "
        "('Jane Smith', 25, 45000.50, '2023-02-20', false)")), SQL_NTS);
    SQLCloseCursor(stmt);

    ret = SQLGetTypeInfo(stmt, SQL_ALL_TYPES);
    fetch_results(stmt, out_file, "SQLGetTypeInfo", ret, true);
    SQLCloseCursor(stmt);

    ret = SQLTables(stmt, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"%", SQL_NTS, (SQLCHAR*)"TABLE", SQL_NTS);
    fetch_results(stmt, out_file, "SQLTables", ret);
    SQLCloseCursor(stmt);

    ret = SQLProcedures(stmt, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"%", SQL_NTS);
    fetch_results(stmt, out_file, "SQLProcedures", ret);
    SQLCloseCursor(stmt);

    ret = SQLProcedureColumns(stmt, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"%", SQL_NTS, (SQLCHAR*)"%", SQL_NTS);
    fetch_results(stmt, out_file, "SQLProcedureColumns", ret);
    SQLCloseCursor(stmt);

    ret = SQLTablePrivileges(stmt, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"test_metadata", SQL_NTS);
    fetch_results(stmt, out_file, "SQLTablePrivileges", ret);
    SQLCloseCursor(stmt);

    ret = SQLColumnPrivileges(stmt, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"test_metadata", SQL_NTS, (SQLCHAR*)"%", SQL_NTS);
    fetch_results(stmt, out_file, "SQLColumnPrivileges", ret);
    SQLCloseCursor(stmt);

    ret = SQLPrimaryKeys(stmt, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"test_metadata", SQL_NTS);
    fetch_results(stmt, out_file, "SQLPrimaryKeys", ret);
    SQLCloseCursor(stmt);

    ret = SQLForeignKeys(stmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"test_metadata", SQL_NTS);
    fetch_results(stmt, out_file, "SQLForeignKeys", ret);
    SQLCloseCursor(stmt);

    // TODO: uncomment after fixing SQLSpecialColumns
    // ret = SQLSpecialColumns(stmt, SQL_BEST_ROWID, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"test_metadata", SQL_NTS, SQL_SCOPE_CURROW, SQL_NULLABLE);
    // fetch_results(stmt, out_file, "SQLSpecialColumns", ret);
    // SQLCloseCursor(stmt);

    ret = SQLStatistics(stmt, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"test_metadata", SQL_NTS, SQL_INDEX_ALL, SQL_QUICK);
    fetch_results(stmt, out_file, "SQLStatistics", ret);
    SQLCloseCursor(stmt);

    // Additional metadata functions in JSON format
    ret = SQLExecDirect(stmt, const_cast<SQLCHAR *>(reinterpret_cast<const SQLCHAR *>("SELECT 1")), SQL_NTS);
    if (ret == SQL_SUCCESS) {
        SQLSMALLINT num_cols;
        ret = SQLNumResultCols(stmt, &num_cols);
        out_file << ",\n  \"SQLNumResultCols\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"column_count\": " << num_cols << "\n";
        out_file << "  }";
        
        if (num_cols > 0) {
            SQLCHAR col_name[MAX_COLUMN_NAME_LEN];
            SQLSMALLINT name_len, data_type, decimal_digits, nullable;
            SQLULEN column_size;
            SQLLEN attr_value;

            ret = SQLDescribeCol(stmt, 1, col_name, sizeof(col_name), &name_len, &data_type, &column_size, &decimal_digits, &nullable);
            out_file << ",\n  \"SQLDescribeCol\": {\n";
            out_file << "    \"return_code\": " << ret << ",\n";
            out_file << "    \"column_name\": \"" << col_name << "\",\n";
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

    ret = SQLPrepare(stmt, const_cast<SQLCHAR *>(reinterpret_cast<const SQLCHAR *>("SELECT ? as int_param, ? as varchar_param, ? as date_param, ? as decimal_param, ? as bool_param")), SQL_NTS);
    if (ret == SQL_SUCCESS) {
        SQLSMALLINT num_params;
        ret = SQLNumParams(stmt, &num_params);
        out_file << ",\n  \"SQLNumParams\": {\n";
        out_file << "    \"return_code\": " << ret << ",\n";
        out_file << "    \"parameter_count\": " << num_params << "\n";
        out_file << "  }";
    }

    out_file << "\n}\n";

    ret = SQLExecDirect(stmt, const_cast<SQLCHAR *>(reinterpret_cast<const SQLCHAR *>(
        "DROP TABLE IF EXISTS test_metadata")), SQL_NTS);
    SQLCloseCursor(stmt);

    out_file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

TEST_P(ODBC_API_TEST, SQLGetInfoTest) {
    std::string test_dsn = GetParam();

    std::ofstream out_file = create_test_file(test_dsn, "SQLGetInfoTest");
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
        
        SQLCHAR buffer[MAX_BUFFER_LEN] = {};
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
            out_file << ",\n    \"value\": \"" << reinterpret_cast<char *>(buffer) << "\"\n";
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

    std::ofstream out_file = create_test_file(test_dsn, "SQLColumnsTest");
    out_file << "{\n";

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    EXPECT_EQ(ret, SQL_SUCCESS);

    ret = SQLColumns(stmt, NULL, 0, (SQLCHAR*)"public", SQL_NTS, (SQLCHAR*)"%", SQL_NTS, (SQLCHAR*)"%", SQL_NTS);
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
            SQLCHAR col_name[MAX_COLUMN_NAME_LEN];
            SQLSMALLINT name_len;
            SQLDescribeCol(stmt, i, col_name, sizeof(col_name), &name_len, NULL, NULL, NULL, NULL);
            out_file << "\"" << col_name << "\"";
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
                SQLCHAR data[MAX_SQL_MESSAGE_LEN] = {};
                SQLLEN indicator = 0;
                SQLRETURN get_ret = SQLGetData(stmt, i, SQL_C_CHAR, data, sizeof(data) - 1, &indicator);
                if (get_ret == SQL_SUCCESS || get_ret == SQL_SUCCESS_WITH_INFO) {
                    if (indicator == SQL_NULL_DATA) {
                        out_file << "null";
                    } else {
                        data[sizeof(data) - 1] = '\0';
                        out_file << "\"" << data << "\"";
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
        out_file << ",\n    \"error\": \"" << get_error_message(SQL_HANDLE_STMT, stmt, ret) << "\"\n";
    }
    out_file << "  }\n";
    SQLCloseCursor(stmt);

    out_file << "}\n";
    out_file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

static std::vector<std::string> getDsnValues() {
    test_server = std::getenv("TEST_SERVER");
    if (!test_server) test_server = const_cast<char *>("localhost");

    const char* port_str = std::getenv("TEST_PORT");
    test_port = port_str ? std::strtol(port_str, nullptr, 10) : 5432;

    test_dsn = std::getenv("TEST_DSN");
    if (!test_dsn) test_dsn = const_cast<char *>("wrapper-dsn");

    test_db = std::getenv("TEST_DATABASE");
    test_uid = std::getenv("TEST_USERNAME");
    test_pwd = std::getenv("TEST_PASSWORD");
    test_base_driver = std::getenv("TEST_BASE_DRIVER");
    test_base_dsn = std::getenv("TEST_BASE_DSN");

    return std::vector<std::string> {test_dsn, test_base_dsn};
}

INSTANTIATE_TEST_SUITE_P(
    MetadataFunctionsTest,
    ODBC_API_TEST,
    testing::ValuesIn(getDsnValues())
);
