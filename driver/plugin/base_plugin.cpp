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

#include "base_plugin.h"

#include "../driver.h"
#include "../odbcapi.h"
#include "../util/connection_string_helper.h"
#include "../util/logger_wrapper.h"
#include "../util/rds_lib_loader.h"

BasePlugin::BasePlugin(DBC *dbc) : BasePlugin(dbc, nullptr) {}

BasePlugin::BasePlugin(DBC *dbc, BasePlugin *next_plugin) :
    next_plugin(next_plugin),
    plugin_name("BasePlugin") {}

BasePlugin::~BasePlugin() {
    if (next_plugin) {
        delete next_plugin;
        next_plugin = nullptr;
    }
}

SQLRETURN BasePlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    SQLRETURN ret = SQL_ERROR;
    bool has_conn_attr_errors = false;
    DBC* dbc = (DBC*) ConnectionHandle;
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
    RDS_STR conn_in = ConnectionStringHelper::BuildMinimumConnectionString(dbc->conn_attr);
    DLOG(INFO) << "Built minimum connection string for underlying driver: " << ToStr(ConnectionStringHelper::MaskSensitiveInformation(conn_in));
    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDriverConnect, RDS_STR_SQLDriverConnect,
        dbc->wrapped_dbc, WindowHandle, AS_SQLTCHAR(conn_in.c_str()), SQL_NTS, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion
    );

    if (res.fn_load_success) {
        ret = res.fn_result;
    }

    // Apply Tracked Connection Attributes
    for (auto const& [key, val] : dbc->attr_map) {
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetConnectAttr, RDS_STR_SQLSetConnectAttr,
            dbc->wrapped_dbc, key, val.first, val.second
        );
        if (!res.fn_result) {
            LOG(WARNING) << "Error setting connection attribute";
        }
        has_conn_attr_errors != res.fn_result;
    }

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

SQLRETURN BasePlugin::Execute(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    SQLRETURN ret = SQL_ERROR;
    RdsLibResult res;
    STMT* stmt = (STMT*) StatementHandle;
    DBC* dbc = stmt->dbc;
    ENV* env = dbc->env;
    RDS_STR query = AS_RDS_STR(StatementText);

    // Allocate wrapped handle if NULL
    if (!stmt->wrapped_stmt) {
        if (dbc->wrapped_dbc) {
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
                SQL_HANDLE_STMT, dbc->wrapped_dbc, &stmt->wrapped_stmt
            );
        } else {
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
        RDS_STR cursor_name = stmt->cursor_name;
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetCursorName, RDS_STR_SQLSetCursorName,
            stmt->wrapped_stmt, AS_SQLTCHAR(cursor_name.c_str()), cursor_name.length()
        );
    }

    if (query.empty()) {
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLExecute, RDS_STR_SQLExecute,
            stmt->wrapped_stmt
        );
    } else {
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
            stmt->wrapped_stmt, StatementText, TextLength
        );
    }

    return res.fn_result;
}
