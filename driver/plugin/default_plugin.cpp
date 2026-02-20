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

#include "default_plugin.h"

#include <iostream>
#include "../driver.h"
#include "../odbcapi.h"
#include "../util/connection_string_helper.h"
#include "../util/logger_wrapper.h"
#include "../util/rds_lib_loader.h"
#include "../util/sql_query_analyzer.h"

DefaultPlugin::DefaultPlugin(DBC *dbc) : DefaultPlugin(dbc, nullptr) {}

DefaultPlugin::DefaultPlugin(DBC *dbc, DefaultPlugin *next_plugin) : plugin_name("DefaultPlugin") {}

SQLRETURN DefaultPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    LOG(INFO) << "Entering Connect";
    SQLRETURN ret = SQL_ERROR;
    bool has_conn_attr_errors = false;
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    ENV* env = dbc->env;

    // TODO - Should a new connect use a new underlying DBC?
    // Create Wrapped DBC if not already allocated
    RdsLibResult res;
    if (!dbc->wrapped_dbc) {
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
            SQL_HANDLE_DBC, env->wrapped_env, &dbc->wrapped_dbc
        );
    }

    // DSN should be read from the original input
    // and a new connection string should be built without DSN & Driver
    const std::string conn_in = ConnectionStringHelper::BuildMinimumConnectionString(dbc->conn_attr);
    DLOG(INFO) << "Built minimum connection string for underlying driver: " << ConnectionStringHelper::MaskSensitiveInformation(conn_in);

    SQLTCHAR *conn_in_sqltchar;
#if UNICODE
    const std::vector<uint16_t> conn_in_vec = ConvertUTF8ToUTF16(conn_in);
    const uint16_t* conn_in_ushort = conn_in_vec.data();

    conn_in_sqltchar = const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(conn_in_ushort));
#else
    conn_in_sqltchar = const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(conn_in.c_str()));
#endif
    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDriverConnect, RDS_STR_SQLDriverConnect,
        dbc->wrapped_dbc, WindowHandle, conn_in_sqltchar, SQL_NTS, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion
    );

    if (res.fn_load_success) {
        ret = res.fn_result;
        if (!SQL_SUCCEEDED(ret)) {
#if UNICODE
            if (dbc->wrapped_dbc) {
                SQLINTEGER nativeError;
                SQLTCHAR state[MAX_SQL_STATE_LEN*2] = {0};
                SQLTCHAR text[MAX_MSG_LEN*2] = {0};
                SQLSMALLINT len;

                NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
                    SQL_HANDLE_DBC, dbc->wrapped_dbc, 1, state, &nativeError, text, MAX_MSG_LEN, &len
                );

                bool found_null_terminator = false;
                for (int i = MAX_SQL_STATE_LEN; i < MAX_SQL_STATE_LEN*2; i++) {
                    if (state[i] != '\0') {
                        env->use_4_bytes = true;
                    }
                }

                if (env->use_4_bytes) {
                    // Try connecting again with 4-byte characters
                    std::wstring wide_conn(conn_in.begin(), conn_in.end());
                    conn_in_sqltchar = const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(wide_conn.c_str()));
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDriverConnect, RDS_STR_SQLDriverConnect,
                        dbc->wrapped_dbc, WindowHandle, conn_in_sqltchar, SQL_NTS, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion
                    );
                    ret = res.fn_result;
                }
            }
#else
            return ret;
#endif
        }
    }

    // Apply Tracked Connection Attributes
    for (auto const& [key, val] : dbc->attr_map) {
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetConnectAttr, RDS_STR_SQLSetConnectAttr,
            dbc->wrapped_dbc, key, val.first, val.second
        );
        if (!res.fn_result) {
            LOG(WARNING) << "Error setting connection attribute";
        }
        has_conn_attr_errors = res.fn_result == 0 ? has_conn_attr_errors : true;
    }
    dbc->transaction_status = dbc->auto_commit ? TRANSACTION_CLOSED : TRANSACTION_OPEN;

    // TODO - Error Handling for ConnAttr, IsConnected
    // Successful Connection, but bad environment and/or connection attribute setting
    if (SQL_SUCCEEDED(ret)) {
        dbc->conn_status = CONN_CONNECTED;
        if (has_conn_attr_errors) {
            ret = SQL_SUCCESS_WITH_INFO;
        }
    }
    return ret;
}

SQLRETURN DefaultPlugin::Execute(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    LOG(INFO) << "Entering Execute";
    RdsLibResult res;
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;
    const std::string query = StatementText ? AS_UTF8_CSTR(StatementText) : "";

    // Allocate wrapped handle if NULL
    if (!stmt->wrapped_stmt) {
        if (dbc->wrapped_dbc) {
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
                SQL_HANDLE_STMT, dbc->wrapped_dbc, &stmt->wrapped_stmt
            );
        } else {
            LOG(ERROR) << "Unable to use STMT, underlying DBC nulled";
            stmt->err = new ERR_INFO("Unable to use STMT, underlying DBC nulled", ERR_UNDERLYING_HANDLE_NULL);
            return SQL_ERROR;
        }
        // Set statement settings
        for (auto const& [key, val] : stmt->attr_map) {
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetStmtAttr, RDS_STR_SQLSetStmtAttr,
                stmt->wrapped_stmt, key, val.first, val.second
            );
        }
        // Cursor Name
        const std::string cursor_name = stmt->cursor_name;
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetCursorName, RDS_STR_SQLSetCursorName,
            stmt->wrapped_stmt, AS_SQLTCHAR(cursor_name), cursor_name.length()
        );
    }

    if (query.empty()) {
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLExecute, RDS_STR_SQLExecute,
            stmt->wrapped_stmt
        );
    } else {
#if UNICODE
        if (env->use_4_bytes) {
            std::wstring wide_conn(query.begin(), query.end());
            SQLTCHAR* conn_in_sqltchar = const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(wide_conn.c_str()));
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
                stmt->wrapped_stmt, conn_in_sqltchar, TextLength
            );
        } else {
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
                stmt->wrapped_stmt, StatementText, TextLength
            );
        }
#else
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
            stmt->wrapped_stmt, StatementText, TextLength
        );
#endif
    }

    // Supports checking for transaction changes only if it was a direct execute
    if (SQL_SUCCEEDED(res.fn_result) && !query.empty()) {
        if (SqlQueryAnalyzer::DoesOpenTransaction(query)) {
            dbc->transaction_status = TRANSACTION_OPEN;
        } else if (SqlQueryAnalyzer::DoesCloseTransaction(dbc, query)
            || SqlQueryAnalyzer::DoesSwitchAutoCommitFalseTrue(dbc, query)
        ) {
            dbc->transaction_status = TRANSACTION_CLOSED;
        }

        if (SqlQueryAnalyzer::IsStatementSettingAutoCommit(query)) {
            dbc->auto_commit = SqlQueryAnalyzer::GetAutoCommitValueFromSqlStatement(query);
            NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetConnectAttr, RDS_STR_SQLSetConnectAttr,
                dbc->wrapped_dbc, SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(dbc->auto_commit), 0
            );
            dbc->attr_map.insert_or_assign(SQL_ATTR_AUTOCOMMIT, std::make_pair(reinterpret_cast<SQLPOINTER>(dbc->auto_commit), 0));
        }
    }
    return res.fn_result;
}
