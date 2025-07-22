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

#include "util/auth_provider.h"

#include "odbcapi_rds_helper.h"

#include "plugin/iam/iam_auth_plugin.h"

#include "util/connection_string_helper.h"
#include "util/connection_string_keys.h"
#include "util/logger_wrapper.h"
#include "util/odbc_dsn_helper.h"
#include "util/rds_lib_loader.h"
#include "util/rds_strings.h"

SQLRETURN RDS_ProcessLibRes(
    SQLSMALLINT    HandleType,
    SQLHANDLE      InputHandle,
    RdsLibResult   LibResult)
{
    if (!LibResult.fn_load_success) {
        switch (HandleType) {
            case SQL_HANDLE_ENV:
                {
                    ENV *env = (ENV*) InputHandle;
                    if (env->err) delete env->err;
                    env->err = new ERR_INFO("Underlying driver failed to load/execute", 12345, "12345");
                    break;
                }
            case SQL_HANDLE_DBC:
                {
                    DBC *dbc = (DBC*) InputHandle;
                    if (dbc->err) delete dbc->err;
                    dbc->err = new ERR_INFO("Underlying driver failed to load/execute", 12345, "12345");
                    break;
                }
            case SQL_HANDLE_STMT:
                {
                    STMT *stmt = (STMT*) InputHandle;
                    if (stmt->err) delete stmt->err;
                    stmt->err = new ERR_INFO("Underlying driver failed to load/execute", 12345, "12345");
                    break;
                }
            case SQL_HANDLE_DESC:
                {
                    DESC *desc = (DESC*) InputHandle;
                    if (desc->err) delete desc->err;
                    desc->err = new ERR_INFO("Underlying driver failed to load/execute", 12345, "12345");
                    break;
                }
            default:
                NOT_IMPLEMENTED;
                break;
        }
    }
    return LibResult.fn_result;
}

SQLRETURN RDS_AllocEnv(
    SQLHENV *      EnvironmentHandlePointer)
{
    LoggerWrapper::Initialize();
    ENV *env;

    env = new ENV();
    *EnvironmentHandlePointer = env;

    return SQL_SUCCESS;
}

SQLRETURN RDS_AllocDbc(
    SQLHENV         EnvironmentHandle,
    SQLHDBC *       ConnectionHandlePointer)
{
    NULL_CHECK_HANDLE(EnvironmentHandle);
    DBC *dbc;
    ENV *env = (ENV*) EnvironmentHandle;

    dbc = new DBC();
    dbc->env = env;
    *ConnectionHandlePointer = dbc;

    env->dbc_list.emplace_back(dbc);

    return SQL_SUCCESS;
}

SQLRETURN RDS_AllocStmt(
    SQLHDBC        ConnectionHandle,
    SQLHSTMT *     StatementHandlePointer)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    STMT *stmt;
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV* env = (ENV*) dbc->env;

    stmt = new STMT();
    stmt->dbc = dbc;
    // Create underlying driver's statement handle
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_STMT, dbc->wrapped_dbc, &stmt->wrapped_stmt
    );
    RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
    *StatementHandlePointer = stmt;

    dbc->stmt_list.emplace_back(stmt);

    return SQL_SUCCESS;
}

SQLRETURN RDS_AllocDesc(
    SQLHDBC        ConnectionHandle,
    SQLHANDLE *    DescriptorHandlePointer)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DESC *desc;
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV* env = (ENV*) dbc->env;

    desc = new DESC();
    desc->dbc = dbc;
    // Create underlying driver's descriptor handle
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_DESC, dbc->wrapped_dbc, &desc->wrapped_desc
    );
    RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
    *DescriptorHandlePointer = desc;

    dbc->desc_list.emplace_back(desc);

    return SQL_SUCCESS;
}

SQLRETURN RDS_SQLEndTran(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    CompletionType)
{
    SQLRETURN ret = SQL_SUCCESS;
    ENV* env = nullptr;
    DBC* dbc = nullptr;
    RdsLibResult res;
    switch (HandleType) {
        case SQL_HANDLE_DBC:
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            {
                dbc = (DBC*) Handle;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

                res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLEndTran, RDS_STR_SQLEndTran,
                    HandleType, Handle, CompletionType
                );
                RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
            }
            break;
        case SQL_HANDLE_ENV:
            NULL_CHECK_HANDLE(Handle);
            {
                env = (ENV*) Handle;

                std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

                for (DBC* dbc : env->dbc_list) {
                    // TODO - May need enhanced error handling due to multiple DBCs
                    //   Should error out on the first?
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLEndTran, RDS_STR_SQLEndTran,
                        HandleType, Handle, CompletionType
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
                }
            }
            break;
        default:
            // TODO - Set error
            ret = SQL_ERROR;
            break;
    }
    return ret;
}

SQLRETURN RDS_FreeConnect(
    SQLHDBC        ConnectionHandle)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    // Remove connection from environment
    env->dbc_list.remove(dbc); // TODO - Make this into a function within ENV to make use of locks

    // Clean up wrapped DBC
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
        SQL_HANDLE_DBC, dbc->wrapped_dbc
    );
    RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);

    if (dbc->plugin_head) delete dbc->plugin_head;
    if (dbc->err) delete dbc->err;

    delete dbc;
    return SQL_SUCCESS;
}

SQLRETURN RDS_FreeDesc(
    SQLHDESC       DescriptorHandle)
{
    NULL_CHECK_ENV_ACCESS_DESC(DescriptorHandle);
    DESC* desc = (DESC*) DescriptorHandle;
    DBC* dbc = desc->dbc;
    ENV* env = dbc->env;

    // Remove descriptor from connection
    dbc->desc_list.remove(desc);

    // Clean underlying Descriptors
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
        SQL_HANDLE_DESC, desc->wrapped_desc
    );
    RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);

    if (desc->err) delete desc->err;

    delete desc;
    return SQL_SUCCESS;
}

SQLRETURN RDS_FreeEnv(
    SQLHENV        EnvironmentHandle)
{
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV* env = (ENV*) EnvironmentHandle;

    // Clean tracked connections
    for (DBC* dbc : env->dbc_list) {
        RDS_FreeConnect(dbc);
    }

    if (env->driver_lib_loader) {
        // Clean underlying Env
        RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
            SQL_HANDLE_ENV, env->wrapped_env
        );
        RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
        env->driver_lib_loader.reset();
    }

    if (env->err) delete env->err;

    LoggerWrapper::Shutdown();

    delete env;
    return SQL_SUCCESS;
}

SQLRETURN RDS_FreeStmt(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = (STMT*) StatementHandle;
    DBC* dbc = stmt->dbc;
    ENV* env = dbc->env;

    // Remove statement from connection
    dbc->stmt_list.remove(stmt);

    // Clean underlying Statements
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
        SQL_HANDLE_STMT, stmt->wrapped_stmt
    );
    RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);

    if (stmt->err) delete stmt->err;

    delete stmt;
    return SQL_SUCCESS;
}

SQLRETURN RDS_GetConnectAttr(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    SQLRETURN ret = SQL_ERROR;

    // If already connected, query value from underlying DBC
    if (dbc->wrapped_dbc) {
        RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetConnectAttr, RDS_STR_SQLGetConnectAttr,
            dbc->wrapped_dbc, Attribute, ValuePtr, BufferLength, StringLengthPtr
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
    }
    // Otherwise get from the DBC's attribute map
    else if (dbc->attr_map.find(Attribute) != dbc->attr_map.end()) {
        std::pair<SQLPOINTER, SQLINTEGER> value_pair = dbc->attr_map.at(Attribute);
        if (value_pair.second == sizeof(SQLSMALLINT)) {
            *((SQLUSMALLINT *) ValuePtr) = static_cast<SQLSMALLINT>(reinterpret_cast<intptr_t>(value_pair.first));
        } else if (value_pair.second == sizeof(SQLUINTEGER) || value_pair.second == 0) {
            *((SQLUINTEGER *) ValuePtr) = static_cast<SQLUINTEGER>(reinterpret_cast<uintptr_t>(value_pair.first));
        } else {
            RDS_sprintf((RDS_CHAR *) ValuePtr, (size_t) BufferLength / sizeof(SQLTCHAR), RDS_CHAR_FORMAT, AS_RDS_CHAR(value_pair.first));
            if (value_pair.second >= BufferLength) {
                    ret = SQL_SUCCESS_WITH_INFO;
            }
        }
        ret = SQL_SUCCESS;
    }
    return ret;
}

SQLRETURN RDS_SQLSetConnectAttr(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    SQLRETURN ret = SQL_ERROR;

    // If already connected, apply value to underlying DBC, otherwise track and apply on connect
    if (dbc->wrapped_dbc) {
        RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetConnectAttr, RDS_STR_SQLSetConnectAttr,
            dbc->wrapped_dbc, Attribute, ValuePtr, StringLength
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
    } else {
        dbc->attr_map.insert_or_assign(Attribute, std::make_pair(ValuePtr, StringLength));
    }

    return ret;
}

// Support for Ansi & Unicode specifics
SQLRETURN RDS_SQLBrowseConnect(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr)
{
    NOT_IMPLEMENTED;
}

SQLRETURN RDS_SQLColAttribute(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLColAttribute, RDS_STR_SQLColAttribute,
        stmt->wrapped_stmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLColAttributes(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLColAttributes, RDS_STR_SQLColAttributes,
        stmt->wrapped_stmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLColumnPrivileges(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    NameLength4)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLColumnPrivileges, RDS_STR_SQLColumnPrivileges,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, ColumnName, NameLength4
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLColumns(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    NameLength4)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLColumns, RDS_STR_SQLColumns,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, ColumnName, NameLength4
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLConnect(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     ServerName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     UserName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     Authentication,
    SQLSMALLINT    NameLength3)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC* dbc = (DBC*) ConnectionHandle;
    ENV* env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    SQLRETURN ret = SQL_ERROR;

    // Connection is already established
    if (CONN_NOT_CONNECTED != dbc->conn_status) {
        // TODO - Error info
        return SQL_ERROR;
    }

    size_t load_len = -1;
    if (ServerName) {
        // Load input DSN followed by Base DSN retrieved from input DSN
        load_len = NameLength1 == SQL_NTS ? RDS_STR_LEN(AS_RDS_CHAR(ServerName)) : NameLength1;
        OdbcDsnHelper::LoadAll(AS_RDS_STR_MAX(ServerName, load_len), dbc->conn_attr);
        ret = RDS_InitializeConnection(dbc);
    } else {
        // Error, no DSN
        // TODO - Load default DSN?
        ret = SQL_ERROR;
    }

    // Replace with input parameters
    if (UserName) {
        load_len = NameLength2 == SQL_NTS ? RDS_STR_LEN(AS_RDS_CHAR(UserName)) : NameLength2;
        dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, AS_RDS_STR_MAX(UserName, load_len));
    }
    if (Authentication) {
        load_len = NameLength3 == SQL_NTS ? RDS_STR_LEN(AS_RDS_CHAR(Authentication)) : NameLength3;
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, AS_RDS_STR_MAX(Authentication, load_len));
    }

    // Connect if initialization successful
    if (SQL_SUCCEEDED(ret)) {
        ret = dbc->plugin_head->Connect(nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    }

    return ret;
}

// TODO Maybe - Impl SQLDataSources
// Both PostgreSQL AND MySQL do NOT implement this
SQLRETURN RDS_SQLDataSources(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLTCHAR *     ServerName,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  NameLength1Ptr,
    SQLTCHAR *     Description,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  NameLength2Ptr)
{
    NOT_IMPLEMENTED;
}

SQLRETURN RDS_SQLDescribeCol(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr,
    SQLSMALLINT *  DataTypePtr,
    SQLULEN *      ColumnSizePtr,
    SQLSMALLINT *  DecimalDigitsPtr,
    SQLSMALLINT *  NullablePtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDescribeCol, RDS_STR_SQLDescribeCol,
        stmt->wrapped_stmt, ColumnNumber, ColumnName, BufferLength, NameLengthPtr, DataTypePtr, ColumnSizePtr, DecimalDigitsPtr, NullablePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLDriverConnect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr,
    SQLUSMALLINT   DriverCompletion)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    SQLRETURN ret = SQL_ERROR;

    // Connection is already established
    if (CONN_NOT_CONNECTED != dbc->conn_status) {
        // TODO - Error info
        return SQL_ERROR;
    }

    // Parse connection string, load input DSN followed by Base DSN
    size_t load_len = StringLength1 == SQL_NTS ? RDS_STR_LEN(AS_RDS_CHAR(InConnectionString)) : StringLength1;
    ConnectionStringHelper::ParseConnectionString(AS_RDS_STR_MAX(InConnectionString, load_len), dbc->conn_attr);

    // Load DSN information into map
    if (dbc->conn_attr.find(KEY_DSN) != dbc->conn_attr.end()) {
        OdbcDsnHelper::LoadAll(dbc->conn_attr.at(KEY_DSN), dbc->conn_attr);
    }

    ret = RDS_InitializeConnection(dbc);

    // Connect if initialization successful
    if (SQL_SUCCEEDED(ret)) {
        ret = dbc->plugin_head->Connect(WindowHandle, OutConnectionString, BufferLength, StringLength2Ptr, DriverCompletion);
    }

    return ret;
}

// TODO Maybe - Impl SQLDrivers
// Not implemented in MySQL or PostgreSQL ODBC
SQLRETURN RDS_SQLDrivers(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLTCHAR *     DriverDescription,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  DescriptionLengthPtr,
    SQLTCHAR *     DriverAttributes,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  AttributesLengthPtr)
{
    NOT_IMPLEMENTED;
}

// TODO Maybe - Impl SQLError
// Not implemented in MySQL or PostgreSQL ODBC
// Deprecated, 2.x should map to SQLDiagRec
SQLRETURN RDS_SQLError(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLTCHAR *     MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr)
{
    NOT_IMPLEMENTED;
}

SQLRETURN RDS_SQLExecDirect(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
        stmt->wrapped_stmt, StatementText, TextLength
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLForeignKeys(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     PKCatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     PKSchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     PKTableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     FKCatalogName,
    SQLSMALLINT    NameLength4,
    SQLTCHAR *     FKSchemaName,
    SQLSMALLINT    NameLength5,
    SQLTCHAR *     FKTableName,
    SQLSMALLINT    NameLength6)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLForeignKeys, RDS_STR_SQLForeignKeys,
        stmt->wrapped_stmt, PKCatalogName, NameLength1, PKSchemaName, NameLength2, PKTableName, NameLength3, FKCatalogName, NameLength4, FKSchemaName, NameLength5, FKTableName, NameLength6
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLGetConnectAttr(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    return RDS_GetConnectAttr(ConnectionHandle, Attribute, ValuePtr, BufferLength, StringLengthPtr);
}

SQLRETURN RDS_SQLGetConnectOption(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr)
{
    return RDS_GetConnectAttr(
        ConnectionHandle,
        Attribute,
        ValuePtr,
        ((Attribute == SQL_ATTR_CURRENT_CATALOG) ? SQL_MAX_OPTION_STRING_LENGTH : 0),
        NULL
    );
}

SQLRETURN RDS_SQLGetCursorName(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CursorName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetCursorName, RDS_STR_SQLGetCursorName,
        stmt->wrapped_stmt, CursorName, BufferLength, NameLengthPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLGetDescField(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    NULL_CHECK_ENV_ACCESS_DESC(DescriptorHandle);
    DESC* desc = (DESC*) DescriptorHandle;
    DBC* dbc = (DBC*) desc->dbc;
    ENV* env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDescField, RDS_STR_SQLGetDescField,
        desc->wrapped_desc, RecNumber, FieldIdentifier, ValuePtr, BufferLength, StringLengthPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
}

SQLRETURN RDS_SQLGetDescRec(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLTCHAR *     Name,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLSMALLINT *  TypePtr,
    SQLSMALLINT *  SubTypePtr,
    SQLLEN *       LengthPtr,
    SQLSMALLINT *  PrecisionPtr,
    SQLSMALLINT *  ScalePtr,
    SQLSMALLINT *  NullablePtr)
{
    NULL_CHECK_ENV_ACCESS_DESC(DescriptorHandle);
    DESC* desc = (DESC*) DescriptorHandle;
    DBC* dbc = (DBC*) desc->dbc;
    ENV* env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDescRec, RDS_STR_SQLGetDescRec,
        desc->wrapped_desc, RecNumber, Name, BufferLength, StringLengthPtr, TypePtr, SubTypePtr, LengthPtr, PrecisionPtr, ScalePtr, NullablePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
}

SQLRETURN RDS_SQLGetDiagField(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    DiagIdentifier,
    SQLPOINTER     DiagInfoPtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr)
{
    DESC *desc = nullptr;
    STMT *stmt = nullptr;
    DBC *dbc = nullptr;
    ENV *env = nullptr;

    RdsLibResult res;
    SQLRETURN ret = SQL_ERROR;
    SQLULEN len = 0, value = 0;
    const char *char_value = nullptr;
    bool has_underlying_driver_alloc = false;

    switch (HandleType) {
        case SQL_HANDLE_ENV:
            NULL_CHECK_HANDLE(Handle);
            {
                std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

                env = (ENV*) Handle;
                if (env->wrapped_env) {
                    has_underlying_driver_alloc = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagField, RDS_STR_SQLGetDiagField,
                        HandleType, env->wrapped_env, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
                }
            }
            break;
        case SQL_HANDLE_DBC:
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            {
                dbc = (DBC*) Handle;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

                if (dbc->wrapped_dbc) {
                    has_underlying_driver_alloc = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagField, RDS_STR_SQLGetDiagField,
                        HandleType, dbc->wrapped_dbc, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
                }
            }
            break;
        case SQL_HANDLE_STMT:
            NULL_CHECK_ENV_ACCESS_STMT(Handle);
            {
                stmt = (STMT*) Handle;
                dbc = (DBC*) stmt->dbc;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

                if (stmt->wrapped_stmt) {
                    has_underlying_driver_alloc = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagField, RDS_STR_SQLGetDiagField,
                        HandleType, stmt->wrapped_stmt, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
                }
            }
            break;
        case SQL_HANDLE_DESC:
            NULL_CHECK_ENV_ACCESS_DESC(Handle);
            {
                desc = (DESC*) Handle;
                dbc = (DBC*) desc->dbc;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

                if (desc->wrapped_desc) {
                    has_underlying_driver_alloc = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagField, RDS_STR_SQLGetDiagField,
                        HandleType, desc->wrapped_desc, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
                }
            }
            break;
        default:
            // Set error, not a supported handle
            NOT_IMPLEMENTED;
    }

    // Given handle did not have an underlying driver handle allocated
    //   TODO - Impl
    if (!has_underlying_driver_alloc) {
        // TODO - Impl
    }

    return ret;
}

SQLRETURN RDS_SQLGetDiagRec(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLTCHAR *     SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLTCHAR *     MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr)
{
    DESC *desc = nullptr;
    STMT *stmt = nullptr;
    DBC *dbc = nullptr;
    ENV *env = nullptr;
    RdsLibResult res;
    SQLRETURN ret = SQL_NO_DATA_FOUND;
    SQLULEN len = 0, value = 0;
    ERR_INFO *err = nullptr;
    bool has_underlying_driver_alloc = false;

    switch (HandleType) {
        case SQL_HANDLE_ENV:
            NULL_CHECK_HANDLE(Handle);
            {
                env = (ENV*) Handle;

                std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

                if (env->wrapped_env) {
                    has_underlying_driver_alloc = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
                        HandleType, env->wrapped_env, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
                } else {
                    err = env->err;
                }
            }
            break;
        case SQL_HANDLE_DBC:
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            {
                dbc = (DBC*) Handle;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

                if (dbc->wrapped_dbc) {
                    has_underlying_driver_alloc = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
                        HandleType, dbc->wrapped_dbc, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
                } else {
                    err = env->err;
                }
            }
            break;
        case SQL_HANDLE_STMT:
            NULL_CHECK_ENV_ACCESS_STMT(Handle);
            {
                stmt = (STMT*) Handle;
                dbc = (DBC*) stmt->dbc;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

                if (stmt->wrapped_stmt) {
                    has_underlying_driver_alloc = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
                        HandleType, stmt->wrapped_stmt, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
                } else {
                    err = env->err;
                }
            }
            break;
        case SQL_HANDLE_DESC:
            NULL_CHECK_ENV_ACCESS_DESC(Handle);
            {
                desc = (DESC*) Handle;
                dbc = (DBC*) desc->dbc;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

                if (desc->wrapped_desc) {
                    has_underlying_driver_alloc = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
                        HandleType, desc->wrapped_desc, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
                } else {
                    err = env->err;
                }
            }
            break;
        default:
            // Set error, not a supported handle
            NOT_IMPLEMENTED;
    }

    // Given handle did not have an underlying driver handle allocated
    //   Use shell driver's stored message
    if (err) {
        if (SQLState) {
            RDS_sprintf((RDS_CHAR *) SQLState, MAX_SQL_STATE_LEN, RDS_CHAR_FORMAT, AS_RDS_CHAR(err->sqlstate));
        }
        if (MessageText) {
            SQLLEN err_len = strlen(err->error_msg);
            RDS_sprintf((RDS_CHAR *) MessageText, (size_t) BufferLength / sizeof(SQLTCHAR), RDS_CHAR_FORMAT, AS_RDS_CHAR(err->error_msg));
            if (err_len >= BufferLength) {
                    ret = SQL_SUCCESS_WITH_INFO;
            }
            if (TextLengthPtr) {
                    *((SQLSMALLINT *) TextLengthPtr) = (SQLSMALLINT) err_len * sizeof(SQLTCHAR);
            }
        }
        if (NativeErrorPtr) {
            *((SQLUINTEGER *) NativeErrorPtr) = (SQLUINTEGER) err->error_num;
        }
    } else if (!has_underlying_driver_alloc) {
        // No Data
        if (SQLState) {
            RDS_sprintf((RDS_CHAR *) SQLState, MAX_SQL_STATE_LEN, RDS_CHAR_FORMAT, NO_DATA_SQL_STATE);
        }
        if (MessageText) {
            RDS_sprintf((RDS_CHAR *) MessageText, 0, RDS_CHAR_FORMAT, NO_DATA_SQL_STATE);
        }
        if (NativeErrorPtr) {
            *((SQLUINTEGER *) NativeErrorPtr) = (SQLUINTEGER) NO_DATA_NATIVE_ERR;
        }
        if (TextLengthPtr) {
            *((SQLSMALLINT *) TextLengthPtr) = (SQLSMALLINT) NO_DATA_NATIVE_ERR;
        }
        ret = SQL_NO_DATA_FOUND;
    }

    return ret;
}

SQLRETURN RDS_SQLGetInfo(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   InfoType,
    SQLPOINTER     InfoValuePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr)
{
    SQLRETURN ret = SQL_ERROR;
    SQLULEN len = 0, value = 0;
    RDS_CHAR *char_value = nullptr;
    RDS_CHAR odbcver[ODBC_VER_SiZE];

    // Query underlying driver if connection is established
    DBC* dbc = (DBC*) ConnectionHandle;

    {
        std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
        if (dbc && dbc->wrapped_dbc) {
            ENV* env = (ENV*) dbc->env;
            RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetInfo, RDS_STR_SQLGetInfo,
                dbc->wrapped_dbc, InfoType, InfoValuePtr, BufferLength, StringLengthPtr
            );
            return RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
        }
    }

    // Get info for shell driver
    switch (InfoType) {
        case SQL_DRIVER_ODBC_VER:
            RDS_sprintf(odbcver, ODBC_VER_SiZE, TEXT("%02x.%02x"), ODBCVER / 256, ODBCVER % 256);
            char_value = odbcver;
            break;
        case SQL_MAX_CONCURRENT_ACTIVITIES:
            len = 0; // No Limit
            break;
        case SQL_ASYNC_DBC_FUNCTIONS:
        case SQL_ASYNC_NOTIFICATION:
            len = 1; // "Supported"
            break;
        // TODO - Add other cases as needed
        default:
            printf("NOT IMPLEMENTED FOR: %d\n", InfoType);
            NOT_IMPLEMENTED;
    }
    ret = SQL_SUCCESS;

    // Pass info back to caller
    if (char_value) {
        len = RDS_STR_LEN(char_value);
        if (InfoValuePtr) {
            RDS_sprintf((RDS_CHAR *) InfoValuePtr, (size_t) BufferLength / sizeof(SQLTCHAR), RDS_CHAR_FORMAT, AS_RDS_CHAR(char_value));
            if (len >= BufferLength) {
                    ret = SQL_SUCCESS_WITH_INFO;
            }
        }
        if (StringLengthPtr) {
            *StringLengthPtr = (SQLSMALLINT) len * sizeof(SQLTCHAR);
        }
    } else {
        if (InfoValuePtr) {
            if (len == sizeof(SQLSMALLINT))
                    *((SQLUSMALLINT *) InfoValuePtr) = (SQLUSMALLINT) value;
            else if (len == sizeof(SQLINTEGER))
                    *((SQLUINTEGER *) InfoValuePtr) = (SQLUINTEGER) value;
        }
    }

    return ret;
}

SQLRETURN RDS_SQLGetStmtAttr(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetStmtAttr, RDS_STR_SQLGetStmtAttr,
        stmt->wrapped_stmt, Attribute, ValuePtr, BufferLength, StringLengthPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLGetTypeInfo(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    DataType)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetTypeInfo, RDS_STR_SQLGetTypeInfo,
        stmt->wrapped_stmt, DataType
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLNativeSql(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     InStatementText,
    SQLINTEGER     TextLength1,
    SQLTCHAR *     OutStatementText,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   TextLength2Ptr)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLNativeSql, RDS_STR_SQLNativeSql,
        dbc->wrapped_dbc, InStatementText, TextLength1, OutStatementText, BufferLength, TextLength2Ptr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
}

SQLRETURN RDS_SQLPrepare(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLPrepare, RDS_STR_SQLPrepare,
        stmt->wrapped_stmt, StatementText, TextLength
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLPrimaryKeys(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLPrimaryKeys, RDS_STR_SQLPrimaryKeys,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLProcedureColumns(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     ProcName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    NameLength4)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLProcedureColumns, RDS_STR_SQLProcedureColumns,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, ProcName, NameLength3, ColumnName, NameLength4
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLProcedures(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     ProcName,
    SQLSMALLINT    NameLength3)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLProcedures, RDS_STR_SQLProcedures,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, ProcName, NameLength3
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLSetConnectOption(
    SQLHDBC        ConnectionHandle,
    SQLSMALLINT    Option,
    SQLPOINTER     Param)
{
    SQLINTEGER value_len = 0;
    if (Option == SQL_ATTR_CURRENT_CATALOG) {
        value_len = SQL_NTS;
    }
    return RDS_SQLSetConnectAttr(ConnectionHandle, Option, Param, value_len);
}

SQLRETURN RDS_SQLSetCursorName(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CursorName,
    SQLSMALLINT    NameLength)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetCursorName, RDS_STR_SQLSetCursorName,
        stmt->wrapped_stmt, CursorName, NameLength
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLSetDescField(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength)
{
    NULL_CHECK_ENV_ACCESS_DESC(DescriptorHandle);
    DESC *desc = (DESC*) DescriptorHandle;
    DBC *dbc = (DBC*) desc->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetDescField, RDS_STR_SQLSetDescField,
        desc->wrapped_desc, RecNumber, FieldIdentifier, ValuePtr, BufferLength
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
}

SQLRETURN RDS_SQLSetStmtAttr(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetStmtAttr, RDS_STR_SQLSetStmtAttr,
        stmt->wrapped_stmt, Attribute, ValuePtr, StringLength
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLSpecialColumns(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    IdentifierType,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLSMALLINT    Scope,
    SQLSMALLINT    Nullable)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSpecialColumns, RDS_STR_SQLSpecialColumns,
        stmt->wrapped_stmt, IdentifierType, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, Scope, Nullable
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLStatistics(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLUSMALLINT   Unique,
    SQLUSMALLINT   Reserved)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLStatistics, RDS_STR_SQLStatistics,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, Unique, Reserved
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLTablePrivileges(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLTablePrivileges, RDS_STR_SQLTablePrivileges,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLTables(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     TableType,
    SQLSMALLINT    NameLength4)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLTables, RDS_STR_SQLTables,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, TableType, NameLength4
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_InitializeConnection(DBC* dbc)
{
    ENV* env = dbc->env;

    bool has_env_attr_errors = false;

    // Remove input DSN & Driver
    // We don't want the underlying connection
    //  to look back to the wrapper
    // Also allows the Base DSN parse driver into map
    dbc->conn_attr.erase(KEY_DSN);
    dbc->conn_attr.erase(KEY_DRIVER);

    // Set the DSN use the Base
    if (dbc->conn_attr.find(KEY_BASE_DSN) != dbc->conn_attr.end()) {
        dbc->conn_attr.insert_or_assign(KEY_DSN, dbc->conn_attr.at(KEY_BASE_DSN));
        // Load Base DSN info, should contain driver to use
        OdbcDsnHelper::LoadAll(dbc->conn_attr.at(KEY_BASE_DSN), dbc->conn_attr);
    }

    // If driver is not loaded from Base DSN, try input base driver
    if (dbc->conn_attr.find(KEY_DRIVER) == dbc->conn_attr.end()
            && dbc->conn_attr.find(KEY_BASE_DRIVER) != dbc->conn_attr.end()) {
        dbc->conn_attr.insert_or_assign(KEY_DRIVER, dbc->conn_attr.at(KEY_BASE_DRIVER));
    }

    if (dbc->conn_attr.find(KEY_DRIVER) != dbc->conn_attr.end()) {
        // TODO - Need to ensure the paths (slashes) are correct per OS
        RDS_STR driver_path = dbc->conn_attr.at(KEY_DRIVER);

        // Load Module to Env if empty
        if (!env->driver_lib_loader) {
            env->driver_lib_loader = std::make_shared<RdsLibLoader>(driver_path);
        } else if (driver_path != env->driver_lib_loader->GetDriverPath()) {
            // TODO - Set Error, can only use 1 underlying driver per Environment
            LOG(ERROR) << "Attempted to load different drivers to the same environment";
            return SQL_ERROR;
        }
    } else {
        // TODO - No underlying driver in ConnStr
        //   set error and return?
        // OR
        //   check if ENV has an underlying driver already
        LOG(ERROR) << "No driver loaded or found in Connection String / DSN";
        NOT_IMPLEMENTED;
    }

    RdsLibResult res;
    SQLRETURN ret = SQL_SUCCESS;

    // Initialize Wrapped ENV
    // Create Wrapped HENV for Wrapped HDBC if not already allocated
    if (!env->wrapped_env) {
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
            SQL_HANDLE_ENV, nullptr, &env->wrapped_env
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
        // Apply Tracked Environment Attributes
        for (auto const& [key, val] : env->attr_map) {
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetEnvAttr, RDS_STR_SQLSetEnvAttr,
                env->wrapped_env, key, val.first, val.second
            );
            ret |= RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
        }
    }

    // Initialize Plugins
    if (!dbc->plugin_head) {
        BasePlugin* plugin_head = new BasePlugin(dbc);
        BasePlugin* next_plugin;

        // TODO - Grabbing which plugins to initialize will come from a KEY=<Plugin_A, ..., Plugin_Z>;

        // Auth Plugins
        if (dbc->conn_attr.find(KEY_AUTH_TYPE) != dbc->conn_attr.end()) {
            AuthType type = AuthProvider::AuthTypeFromString(dbc->conn_attr.at(KEY_AUTH_TYPE));
            switch (type) {
                    case AuthType::IAM:
                        next_plugin = new IamAuthPlugin(dbc, plugin_head);
                        plugin_head = next_plugin;
                        break;
                    case AuthType::SECRETS_MANAGER:
                        break;
                    case AuthType::ADFS:
                        break;
                    case AuthType::OKTA:
                        break;
                    case AuthType::DATABASE:
                    case AuthType::INVALID:
                    default:
                        break;
            }
        }

        // Finalize and track in DBC
        dbc->plugin_head = plugin_head;
    }

    return SQL_SUCCESS;
}
