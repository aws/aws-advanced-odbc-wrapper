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


#include <cstdio>
#include <cwchar>
#include <unordered_set>

#include "plugin/failover/failover_plugin.h"
#include "plugin/federated/adfs_auth_plugin.h"
#include "plugin/federated/okta_auth_plugin.h"
#include "plugin/iam/iam_auth_plugin.h"
#include "plugin/limitless/limitless_plugin.h"
#include "plugin/secrets_manager/secrets_manager_plugin.h"

#include "util/attribute_validator.h"
#include "util/connection_string_helper.h"
#include "util/connection_string_keys.h"
#include "util/logger_wrapper.h"
#include "util/odbc_dsn_helper.h"
#include "util/rds_lib_loader.h"
#include "util/rds_strings.h"

#ifdef WIN32
    #include "gui/setup.h"
    #include "gui/resource.h"
#endif

SQLRETURN RDS_ProcessLibRes(
    SQLSMALLINT         HandleType,
    SQLHANDLE           InputHandle,
    const RdsLibResult& LibResult)
{
    if (!LibResult.fn_load_success) {
        ERR_INFO* new_err = new ERR_INFO(
            ("Underlying driver failed to load/execute: " + LibResult.fn_name).c_str(),
            ERR_NO_UNDER_LYING_FUNCTION);
        LOG(ERROR) << new_err->error_msg;
        switch (HandleType) {
            case SQL_HANDLE_ENV:
                {
                    ENV *env = static_cast<ENV*>(InputHandle);
                    CLEAR_ENV_ERROR(env);
                    env->err = new_err;
                    break;
                }
            case SQL_HANDLE_DBC:
                {
                    DBC *dbc = static_cast<DBC*>(InputHandle);
                    CLEAR_DBC_ERROR(dbc);
                    dbc->err = new_err;
                    break;
                }
            case SQL_HANDLE_STMT:
                {
                    STMT *stmt = static_cast<STMT*>(InputHandle);
                    CLEAR_STMT_ERROR(stmt);
                    stmt->err = new_err;
                    break;
                }
            case SQL_HANDLE_DESC:
                {
                    DESC *desc = static_cast<DESC*>(InputHandle);
                    CLEAR_DESC_ERROR(desc);
                    desc->err = new_err;
                    break;
                }
            default:
                delete new_err;
                return SQL_INVALID_HANDLE;
                break;
        }
    }
    return LibResult.fn_result;
}

SQLRETURN RDS_AllocEnv(
    SQLHENV *      EnvironmentHandlePointer)
{
    LoggerWrapper::Initialize();
    ENV* env;

    env = new ENV();
    *EnvironmentHandlePointer = env;

    return SQL_SUCCESS;
}

SQLRETURN RDS_AllocDbc(
    SQLHENV        EnvironmentHandle,
    SQLHDBC *      ConnectionHandlePointer)
{
    NULL_CHECK_HANDLE(EnvironmentHandle);
    DBC *dbc;
    ENV *env = static_cast<ENV*>(EnvironmentHandle);

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
    STMT* stmt;
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const ENV* env = dbc->env;

    CHECK_WRAPPED_DBC(dbc);

    stmt = new STMT();
    stmt->dbc = dbc;
    // Create underlying driver's statement handle
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_STMT, dbc->wrapped_dbc, &stmt->wrapped_stmt
    );
    RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
    *StatementHandlePointer = stmt;

    stmt->app_row_desc = new DESC();
    stmt->app_row_desc->dbc = dbc;
    stmt->app_param_desc = new DESC();
    stmt->app_param_desc->dbc = dbc;
    stmt->imp_row_desc = new DESC();
    stmt->imp_row_desc->dbc = dbc;
    stmt->imp_param_desc = new DESC();
    stmt->imp_param_desc->dbc = dbc;

    dbc->stmt_list.emplace_back(stmt);

    return SQL_SUCCESS;
}

SQLRETURN RDS_AllocDesc(
    SQLHDBC        ConnectionHandle,
    SQLHANDLE *    DescriptorHandlePointer)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DESC* desc;
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const ENV* env = dbc->env;

    CHECK_WRAPPED_DBC(dbc);

    desc = new DESC();
    desc->dbc = dbc;
    // Create underlying driver's descriptor handle
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_DESC, dbc->wrapped_dbc, &desc->wrapped_desc
    );
    RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
    *DescriptorHandlePointer = desc;

    dbc->desc_list.emplace_back(desc);

    return SQL_SUCCESS;
}

SQLRETURN RDS_SQLSetEnvAttr(
    SQLHENV        EnvironmentHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV* env = static_cast<ENV*>(EnvironmentHandle);
    SQLRETURN ret = SQL_SUCCESS;

    const std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

    // Track new value
    env->attr_map.insert_or_assign(Attribute, std::make_pair(ValuePtr, StringLength));

    // Check if underlying library is loaded
    //  Don't fail if it isn't loaded as
    //  this can be called prior to connecting
    if (env->driver_lib_loader && env->wrapped_env) {
        // Update existing connections environments
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetEnvAttr, RDS_STR_SQLSetEnvAttr,
            env->wrapped_env, Attribute, ValuePtr, StringLength
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
    }

    return ret;
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
                dbc = static_cast<DBC*>(Handle);
                env = dbc->env;

                const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
                CLEAR_DBC_ERROR(dbc);

                CHECK_WRAPPED_DBC(dbc);
                res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLEndTran, RDS_STR_SQLEndTran,
                    HandleType, dbc->wrapped_dbc, CompletionType
                );
                ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
                if (SQL_SUCCEEDED(ret) && dbc) {
                    dbc->transaction_status = TRANSACTION_CLOSED;
                }
            }
            break;
        case SQL_HANDLE_ENV:
            NULL_CHECK_HANDLE(Handle);
            {
                env = static_cast<ENV*>(Handle);

                const std::lock_guard<std::recursive_mutex> lock_guard(env->lock);
                CLEAR_ENV_ERROR(env);

                for (DBC* dbc : env->dbc_list) {
                    // TODO - May need enhanced error handling due to multiple DBCs
                    //   Should error out on the first?
                    const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
                    CLEAR_DBC_ERROR(dbc);

                    CHECK_WRAPPED_DBC(dbc);
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLEndTran, RDS_STR_SQLEndTran,
                        SQL_HANDLE_DBC, dbc->wrapped_dbc, CompletionType
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
                    if (SQL_SUCCEEDED(ret) && dbc) {
                        dbc->transaction_status = TRANSACTION_CLOSED;
                    }
                }

                res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLEndTran, RDS_STR_SQLEndTran,
                    HandleType, env->wrapped_env, CompletionType
                );
                ret = RDS_ProcessLibRes(HandleType, env, res);
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
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    ENV* env = dbc->env;

    // Remove connection from environment
    env->dbc_list.remove(dbc); // TODO - Make this into a function within ENV to make use of locks

    // Cleanup tracked statements
    const std::list<STMT*> stmt_list = dbc->stmt_list;
    for (STMT* stmt : stmt_list) {
        RDS_FreeStmt(stmt, SQL_DROP);
    }
    dbc->stmt_list.clear();
    // and descriptors
    const std::list<DESC*> desc_list = dbc->desc_list;
    for (DESC* desc : desc_list) {
        RDS_FreeDesc(desc);
    }
    dbc->desc_list.clear();

    // Clean up wrapped DBC
    if (dbc->wrapped_dbc) {
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
            SQL_HANDLE_DBC, dbc->wrapped_dbc
        );
        RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
        dbc->wrapped_dbc = nullptr;
    }

    delete dbc->plugin_head;
    CLEAR_DBC_ERROR(dbc);
    delete dbc;
    return SQL_SUCCESS;
}

SQLRETURN RDS_FreeDesc(
    SQLHDESC       DescriptorHandle)
{
    NULL_CHECK_ENV_ACCESS_DESC(DescriptorHandle);
    DESC* desc = static_cast<DESC*>(DescriptorHandle);
    DBC* dbc = desc->dbc;
    const ENV* env = dbc->env;

    // Remove descriptor from connection
    dbc->desc_list.remove(desc);

    // Clean underlying Descriptors
    if (desc->wrapped_desc) {
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
            SQL_HANDLE_DESC, desc->wrapped_desc
        );
        RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
        desc->wrapped_desc = nullptr;
    }

    CLEAR_DESC_ERROR(desc);
    delete desc;
    return SQL_SUCCESS;
}

SQLRETURN RDS_FreeEnv(
    SQLHENV        EnvironmentHandle)
{
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV* env = static_cast<ENV*>(EnvironmentHandle);

    // Clean tracked connections
    const std::list<DBC*> dbc_list = env->dbc_list;
    for (DBC* dbc : dbc_list) {
        RDS_FreeConnect(dbc);
    }
    env->dbc_list.clear();

    if (env->driver_lib_loader) {
        // Clean underlying Env
        if (env->wrapped_env) {
            const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
                SQL_HANDLE_ENV, env->wrapped_env
            );
            RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
            env->wrapped_env = nullptr;
        }
        env->driver_lib_loader.reset();
    }

    CLEAR_ENV_ERROR(env);
    delete env;
    LoggerWrapper::Shutdown();
    return SQL_SUCCESS;
}

SQLRETURN RDS_FreeStmt(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Option)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    switch (Option) {
        case SQL_CLOSE:
        case SQL_UNBIND:
        case SQL_RESET_PARAMS:
            {
                SQLRETURN ret = SQL_ERROR;
                if (stmt->wrapped_stmt) {
                    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeStmt, RDS_STR_SQLFreeStmt,
                        stmt->wrapped_stmt, Option
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
                }
                return ret;
            }
        case SQL_DROP:
            // Deprecated option, used fully cleanup handle
        default:
            {
                // Remove statement from connection
                dbc->stmt_list.remove(stmt);

                // Clean underlying Statements
                if (stmt->wrapped_stmt) {
                    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
                        SQL_HANDLE_STMT, stmt->wrapped_stmt
                    );
                    RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
                    stmt->wrapped_stmt = nullptr;
                }

                delete stmt->app_row_desc;
                stmt->app_row_desc = SQL_NULL_HANDLE;
                delete stmt->app_param_desc;
                stmt->app_param_desc = SQL_NULL_HANDLE;
                delete stmt->imp_row_desc;
                stmt->imp_row_desc = SQL_NULL_HANDLE;
                delete stmt->imp_param_desc;
                stmt->imp_param_desc = SQL_NULL_HANDLE;

                delete stmt->err;
                delete stmt;
                return SQL_SUCCESS;
            }
    }
}

SQLRETURN RDS_GetConnectAttr(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
    CLEAR_DBC_ERROR(dbc);

    SQLRETURN ret = SQL_ERROR;

    // If already connected, query value from underlying DBC
    if (dbc->wrapped_dbc) {
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetConnectAttr, RDS_STR_SQLGetConnectAttr,
            dbc->wrapped_dbc, Attribute, ValuePtr, BufferLength, StringLengthPtr
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
    }
    // Otherwise get from the DBC's attribute map
    else if (dbc->attr_map.contains(Attribute)) {
        ret = SQL_SUCCESS;
        const std::pair<SQLPOINTER, SQLINTEGER> value_pair = dbc->attr_map.at(Attribute);
        if (value_pair.second == sizeof(SQLSMALLINT)) {
            *(static_cast<SQLUSMALLINT*>(ValuePtr)) = static_cast<SQLSMALLINT>(reinterpret_cast<intptr_t>(value_pair.first));
        } else if (value_pair.second == sizeof(SQLUINTEGER) || value_pair.second == 0) {
            *(static_cast<SQLUINTEGER*>(ValuePtr)) = static_cast<SQLUINTEGER>(reinterpret_cast<uintptr_t>(value_pair.first));
        } else {
            snprintf(static_cast<RDS_CHAR*>(ValuePtr), static_cast<size_t>(BufferLength) / sizeof(SQLTCHAR), RDS_CHAR_FORMAT, value_pair.first);
            if (value_pair.second >= BufferLength) {
                ret = SQL_SUCCESS_WITH_INFO;
            }
        }
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
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const ENV* env = dbc->env;

    SQLRETURN ret = SQL_SUCCESS;

    const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
    CLEAR_DBC_ERROR(dbc);

    // If already connected, apply value to underlying DBC, otherwise track and apply on connect
    if (dbc->wrapped_dbc) {
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetConnectAttr, RDS_STR_SQLSetConnectAttr,
            dbc->wrapped_dbc, Attribute, ValuePtr, StringLength
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
    }
    dbc->attr_map.insert_or_assign(Attribute, std::make_pair(ValuePtr, StringLength));

    if (SQL_ATTR_AUTOCOMMIT == Attribute) {
        dbc->auto_commit = reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON) == ValuePtr;
        dbc->transaction_status = dbc->auto_commit ? TRANSACTION_CLOSED : TRANSACTION_OPEN;
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
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    LOG(ERROR) << "Unsupported SQL API - SQLBrowseConnect";
    CLEAR_DBC_ERROR(dbc);
    dbc->err = new ERR_INFO("SQLBrowseConnect - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLColAttribute, RDS_STR_SQLColAttribute,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLColAttributes, RDS_STR_SQLColAttributes,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLColumnPrivileges, RDS_STR_SQLColumnPrivileges,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLColumns, RDS_STR_SQLColumns,
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
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
    CLEAR_DBC_ERROR(dbc);

    SQLRETURN ret = SQL_ERROR;

    // Connection is already established
    if (CONN_NOT_CONNECTED != dbc->conn_status) {
        // TODO - Error info
        return SQL_ERROR;
    }

    std::string conn_str_utf8;
    size_t load_len = -1;
    if (ServerName) {
        // Load input DSN followed by Base DSN retrieved from input DSN
#ifdef UNICODE
        conn_str_utf8 = ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(ServerName));
#else
        conn_str_utf8 = reinterpret_cast<const char *>(ServerName);
#endif
        load_len = NameLength1 == SQL_NTS ? conn_str_utf8.length() : NameLength1;
        OdbcDsnHelper::LoadAll(conn_str_utf8.substr(0, load_len), dbc->conn_attr);
        ret = RDS_InitializeConnection(dbc);
    } else {
        // Error, no DSN
        // TODO - Load default DSN?
        ret = SQL_ERROR;
    }

    // Replace with input parameters
    if (UserName) {
#ifdef UNICODE
        conn_str_utf8 = ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(UserName));
#else
        conn_str_utf8 = reinterpret_cast<const char *>(UserName);
#endif
        load_len = NameLength2 == SQL_NTS ? conn_str_utf8.length() : NameLength2;
        dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, conn_str_utf8.substr(0, load_len));
    }
    if (Authentication) {
#ifdef UNICODE
        conn_str_utf8 = ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(Authentication));
#else
        conn_str_utf8 = reinterpret_cast<const char *>(Authentication);
#endif
        load_len = NameLength3 == SQL_NTS ? conn_str_utf8.length() : NameLength3;
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, conn_str_utf8.substr(0, load_len));
    }

    // Connect if initialization successful
    if (SQL_SUCCEEDED(ret)) {
        ret = dbc->plugin_head->Connect(ConnectionHandle, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
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
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV *env = static_cast<ENV*>(EnvironmentHandle);

    LOG(ERROR) << "Unsupported SQL API - SQLDataSources";
    CLEAR_ENV_ERROR(env);
    env->err = new ERR_INFO("SQLDataSources - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDescribeCol, RDS_STR_SQLDescribeCol,
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
    std::string conn_str_utf8;
    std::string conn_out_str_utf8;

    if (DriverCompletion && WindowHandle) {
#if _WIN32
        bool complete_required = true;
        std::tuple<std::string, std::string, bool> dialog_result; // conn_str, out_conn_str, dialog_box_cancelled

        switch (DriverCompletion) {
            case SQL_DRIVER_PROMPT:
            case SQL_DRIVER_COMPLETE:
                complete_required = false;
            case SQL_DRIVER_COMPLETE_REQUIRED:
                dialog_result = StartDialogForSqlDriverConnect(WindowHandle, InConnectionString, OutConnectionString, complete_required);
                break;
            case SQL_DRIVER_NOPROMPT:
            default:
                break;
        }

        if (std::get<2>(dialog_result)) {
            // If a dialog box was opened but cancelled mid-operation, do not continue with the connection attempt.
            return SQL_NO_DATA_FOUND;
        }

        conn_str_utf8 = std::get<0>(dialog_result);
        conn_out_str_utf8 = std::get<1>(dialog_result);
#endif
    } else {
#ifdef UNICODE
        conn_str_utf8 = ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(InConnectionString));
#else
        conn_str_utf8 = reinterpret_cast<const char *>(InConnectionString);
#endif
    }

    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
    CLEAR_DBC_ERROR(dbc);

    SQLRETURN ret = SQL_ERROR;

    // Connection is already established
    if (CONN_NOT_CONNECTED != dbc->conn_status) {
        // TODO - Error info
        LOG(ERROR) << "Connection handle is already has an existing connection";
        return SQL_ERROR;
    }

    // Parse connection string, load input DSN followed by Base DSN
    ConnectionStringHelper::ParseConnectionString(conn_str_utf8, dbc->conn_attr);

    // Load DSN information into map
    if (dbc->conn_attr.contains(KEY_DSN)) {
        OdbcDsnHelper::LoadAll(dbc->conn_attr.at(KEY_DSN), dbc->conn_attr);
    }

    // Construct Out Connection String if not built by dialog prompt
    // Build without loading base DSN information, connection string is to be used for wrapper
    if (conn_out_str_utf8.empty()) {
        conn_out_str_utf8 = ConnectionStringHelper::BuildFullConnectionString(dbc->conn_attr);
    }

    ret = RDS_InitializeConnection(dbc);
    // Connect if initialization successful
    if (SQL_SUCCEEDED(ret)) {
        // Pass SQL_DRIVER_NOPROMPT to base driver, otherwise base driver may show its own dialog box when it's not needed.
        ret = dbc->plugin_head->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLength2Ptr, SQL_DRIVER_NOPROMPT);
    }

    // Update OutConnectionString with parsed values
    if (OutConnectionString && StringLength2Ptr) {
        const SQLULEN len = conn_out_str_utf8.length();
#ifdef UNICODE
    #if _WIN32
        const SQLULEN written = swprintf(OutConnectionString, MAX_KEY_SIZE, RDS_TSTR(RDS_WCHAR_FORMAT).c_str(), ConvertUTF8ToWString(conn_out_str_utf8).c_str());
    #else
        const SQLULEN written = CopyUTF8ToUTF16Buffer(OutConnectionString, MAX_KEY_SIZE, conn_out_str_utf8);
    #endif
#else
        const SQLULEN written = snprintf(AS_CHAR(OutConnectionString), MAX_KEY_SIZE, RDS_CHAR_FORMAT, conn_out_str_utf8.c_str());
#endif
        if (len >= BufferLength && SQL_SUCCEEDED(ret)) {
            ret = SQL_SUCCESS_WITH_INFO;
        }
        *StringLength2Ptr = static_cast<SQLSMALLINT>(written);
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
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV* env = static_cast<ENV*>(EnvironmentHandle);

    LOG(ERROR) << "Unsupported SQL API - SQLDrivers";
    CLEAR_ENV_ERROR(env);
    env->err = new ERR_INFO("SQLDrivers - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
    NOT_IMPLEMENTED;
}

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
    SQLRETURN ret = SQL_INVALID_HANDLE;

    // Take in order of lowest to high
    //  Stmt > Dbc > Env
    // SQLError is deprecated, if called directly,
    //  return the first error only once
    //  UnixODBC will enter an infinite loop otherwise
    if (StatementHandle) {
        ret = RDS_SQLGetDiagRec(SQL_HANDLE_STMT, StatementHandle, NEXT_STMT_ERROR(StatementHandle),
            SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
        );
    }
    else if (ConnectionHandle) {
        ret = RDS_SQLGetDiagRec(SQL_HANDLE_DBC, ConnectionHandle, NEXT_DBC_ERROR(ConnectionHandle),
            SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
        );
    }
    else if (EnvironmentHandle) {
        ret = RDS_SQLGetDiagRec(SQL_HANDLE_ENV, EnvironmentHandle, NEXT_ENV_ERROR(EnvironmentHandle),
            SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
        );
    }

    return ret;
}

SQLRETURN RDS_SQLExecDirect(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    DBC* dbc = stmt->dbc;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    if (dbc->plugin_head) {
        return dbc->plugin_head->Execute(StatementHandle, StatementText, TextLength);
    }

    LOG(ERROR) << "Cannot execute without an open connection";
    stmt->err = new ERR_INFO("SQLExecDirect - Connection not open", ERR_CONNECTION_NOT_OPEN);
    return SQL_ERROR;
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLForeignKeys, RDS_STR_SQLForeignKeys,
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
    SQLUSMALLINT   Attribute,
    SQLPOINTER     ValuePtr)
{
    return RDS_GetConnectAttr(
        ConnectionHandle,
        Attribute,
        ValuePtr,
        ((Attribute == SQL_ATTR_CURRENT_CATALOG) ? SQL_MAX_OPTION_STRING_LENGTH : 0),
        nullptr
    );
}

SQLRETURN RDS_SQLGetCursorName(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CursorName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;
    SQLRETURN ret = SQL_SUCCESS;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    if (stmt->wrapped_stmt) {
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetCursorName, RDS_STR_SQLGetCursorName,
            stmt->wrapped_stmt, CursorName, BufferLength, NameLengthPtr
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
    } else {
        if (stmt->cursor_name.empty()) {
            stmt->cursor_name = RDS_NUM_APPEND(RDS_STR("CUR_"), stmt->dbc->unnamed_cursor_count++);
        }
        const RDS_STR name = stmt->cursor_name;
        const SQLULEN len = name.length();
        if (CursorName) {
            snprintf(reinterpret_cast<RDS_CHAR*>(CursorName), static_cast<size_t>(BufferLength) / sizeof(SQLTCHAR), RDS_CHAR_FORMAT, name.c_str());
            if (len >= BufferLength) {
                ret = SQL_SUCCESS_WITH_INFO;
            }
        }
        if (NameLengthPtr) {
            *NameLengthPtr = static_cast<SQLSMALLINT>(len * sizeof(SQLTCHAR));
        }
    }

    return ret;
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
    DESC* desc = static_cast<DESC*>(DescriptorHandle);
    const DBC* dbc = desc->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

    CHECK_WRAPPED_DESC(desc);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDescField, RDS_STR_SQLGetDescField,
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
    DESC* desc = static_cast<DESC*>(DescriptorHandle);
    const DBC* dbc = desc->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

    CHECK_WRAPPED_DESC(desc);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDescRec, RDS_STR_SQLGetDescRec,
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
    DESC* desc = nullptr;
    STMT* stmt = nullptr;
    DBC* dbc = nullptr;
    ENV* env = nullptr;

    const ERR_INFO* err = nullptr;

    RdsLibResult res;
    SQLRETURN ret = SQL_SUCCESS;
    SQLULEN len = 0;

    switch (HandleType) {
            NULL_CHECK_ENV(Handle);
            {
                env = static_cast<ENV*>(Handle);
                const std::lock_guard<std::recursive_mutex> lock_guard(env->lock);
                if (env->wrapped_env) {
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagField, RDS_STR_SQLGetDiagField,
                        HandleType, env->wrapped_env, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
                } else if (env->err) {
                    err = new ERR_INFO(*env->err);
                }
            }
            break;
        case SQL_HANDLE_DBC:
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            {
                dbc = static_cast<DBC*>(Handle);
                env = dbc->env;
                const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
                if (dbc->wrapped_dbc) {
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagField, RDS_STR_SQLGetDiagField,
                        HandleType, dbc->wrapped_dbc, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
                } else if (dbc->err) {
                    err = new ERR_INFO(*dbc->err);
                }
            }
            break;
        case SQL_HANDLE_STMT:
            NULL_CHECK_ENV_ACCESS_STMT(Handle);
            {
                stmt = static_cast<STMT*>(Handle);
                dbc = stmt->dbc;
                env = dbc->env;
                const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
                if (stmt->wrapped_stmt) {
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagField, RDS_STR_SQLGetDiagField,
                        HandleType, stmt->wrapped_stmt, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
                } else if (stmt->err) {
                    err = new ERR_INFO(*stmt->err);
                }
            }
            break;
        case SQL_HANDLE_DESC:
            NULL_CHECK_ENV_ACCESS_DESC(Handle);
            {
                desc = static_cast<DESC*>(Handle);
                dbc = desc->dbc;
                env = dbc->env;

                const std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

                if (desc->wrapped_desc) {
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagField, RDS_STR_SQLGetDiagField,
                        HandleType, desc->wrapped_desc, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
                } else if (desc->err) {
                    err = new ERR_INFO(*desc->err);
                }
            }
            break;
        default:
            return SQL_INVALID_HANDLE;
    }

    // Given handle did not have an underlying driver handle allocated
    if (err) {
        if (RecNumber > 1) {
            ret = SQL_NO_DATA_FOUND;
        } else {
            const RDS_CHAR* char_value = nullptr;
            ret = SQL_SUCCESS;
            switch (DiagIdentifier) {
                // Header Fields
                case SQL_DIAG_CURSOR_ROW_COUNT:
                case SQL_DIAG_DYNAMIC_FUNCTION:
                case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
                case SQL_DIAG_ROW_COUNT:
                {
                    // If we're calling these at this point
                    //  we are not connected to an underlying driver
                    ret = SQL_ERROR;
                    break;
                }
                case SQL_DIAG_NUMBER:
                {
                    // No support for multi-error / list
                    //  at most 1 for our Wrapper Layer
                    *(static_cast<SQLINTEGER*>(DiagInfoPtr)) = 1;
                    break;
                }
                case SQL_DIAG_RETURNCODE:
                    // Driver manager's response
                    break;

                // Record Fields
                case SQL_DIAG_CLASS_ORIGIN:
                {
                    if (RecNumber <= 0) {
                        ret = SQL_ERROR;
                    } else {
                        if (err->sqlstate != nullptr && err->sqlstate[0] == 'I' && err->sqlstate[1] == 'M') {
                            char_value = "ODBC 3.0";
                        } else {
                            char_value = "ISO 9075";
                        }
                    }
                    break;
                }
                case SQL_DIAG_COLUMN_NUMBER:
                {
                    if (RecNumber <= 0) {
                        ret = SQL_ERROR;
                    } else {
                        *(static_cast<SQLLEN*>(DiagInfoPtr)) = SQL_ROW_NUMBER_UNKNOWN;
                    }
                    break;
                }
                case SQL_DIAG_CONNECTION_NAME:
                {
                    if (RecNumber <= 0) {
                        ret = SQL_ERROR;
                    }
                    if (dbc != nullptr && dbc->conn_attr.contains(KEY_DSN)) {
                        char_value = dbc->conn_attr.at(KEY_DSN).c_str();
                    }
                    break;
                }
                case SQL_DIAG_MESSAGE_TEXT:
                {
                    if (RecNumber <= 0) {
                        ret = SQL_ERROR;
                    }
                    if (err->error_msg != nullptr) {
                        char_value = AS_RDS_CHAR(err->error_msg);
                    }
                    break;
                }
                case SQL_DIAG_NATIVE:
                {
                    *(static_cast<SQLINTEGER*>(DiagInfoPtr)) = err->native_err;
                    break;
                }
                case SQL_DIAG_ROW_NUMBER:
                {
                    if (RecNumber <= 0) {
                        ret = SQL_ERROR;
                    } else {
                        *(static_cast<SQLLEN*>(DiagInfoPtr)) = SQL_ROW_NUMBER_UNKNOWN;
                    }
                    break;
                }
                case SQL_DIAG_SERVER_NAME:
                {
                    if (dbc != nullptr && dbc->conn_attr.contains(KEY_SERVER)) {
                        char_value = dbc->conn_attr.at(KEY_SERVER).c_str();
                    }
                    break;
                }
                case SQL_DIAG_SQLSTATE:
                {
                    if (RecNumber <= 0) {
                        ret = SQL_ERROR;
                    } else {
                        char_value = AS_RDS_CHAR(err->sqlstate);
                    }
                    break;
                }
                case SQL_DIAG_SUBCLASS_ORIGIN:
                {
                    if (RecNumber <= 0) {
                        ret = SQL_ERROR;
                    }
                    if (err->is_odbc3_subclass) {
                        char_value = "ODBC 3.0";
                    } else {
                        char_value = "ISO 9075";
                    }
                    break;
                }
                default:
                    ret = SQL_ERROR;
            }

            // Pass info back to caller
            if (char_value) {
                len = RDS_STR_LEN(char_value);
                if (DiagInfoPtr) {
                    snprintf(static_cast<RDS_CHAR*>(DiagInfoPtr), static_cast<size_t>(BufferLength) / sizeof(SQLTCHAR), RDS_CHAR_FORMAT, char_value);
                    if (len >= BufferLength) {
                        ret = SQL_SUCCESS_WITH_INFO;
                    }
                } else {
                    ret = SQL_SUCCESS_WITH_INFO;
                }
                if (StringLengthPtr) {
                    *StringLengthPtr = static_cast<SQLSMALLINT>(len * sizeof(SQLTCHAR));
                }
            }
        }
        delete err;
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
    DESC* desc = nullptr;
    STMT* stmt = nullptr;
    DBC* dbc = nullptr;
    ENV* env = nullptr;
    RdsLibResult res;
    SQLRETURN ret = SQL_ERROR;
    const ERR_INFO* err = nullptr;
    bool has_underlying_data = false;

    // Use ERR from Wrapper if exist
    //  otherwise try underlying driver
    switch (HandleType) {
        case SQL_HANDLE_ENV:
            NULL_CHECK_HANDLE(Handle);
            {
                env = static_cast<ENV*>(Handle);

                const std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

                if (env->err && env->err->sqlstate && env->err->error_msg) {
                    err = new ERR_INFO(*env->err);
                } else if (env->wrapped_env) {
                    has_underlying_data = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
                        HandleType, env->wrapped_env, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
                }
            }
            break;
        case SQL_HANDLE_DBC:
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            {
                dbc = static_cast<DBC*>(Handle);
                env = dbc->env;

                const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

                if (dbc->err && dbc->err->sqlstate && dbc->err->error_msg) {
                    err = new ERR_INFO(*dbc->err);
                } else if (dbc->wrapped_dbc) {
                    has_underlying_data = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
                        HandleType, dbc->wrapped_dbc, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
                }
            }
            break;
        case SQL_HANDLE_STMT:
            NULL_CHECK_ENV_ACCESS_STMT(Handle);
            {
                stmt = static_cast<STMT*>(Handle);
                dbc = stmt->dbc;
                env = dbc->env;

                const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

                if (stmt->err && stmt->err->sqlstate && stmt->err->error_msg) {
                    err = new ERR_INFO(*stmt->err);
                } else if (stmt->wrapped_stmt) {
                    has_underlying_data = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
                        HandleType, stmt->wrapped_stmt, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
                }
            }
            break;
        case SQL_HANDLE_DESC:
            NULL_CHECK_ENV_ACCESS_DESC(Handle);
            {
                desc = static_cast<DESC*>(Handle);
                dbc = desc->dbc;
                env = dbc->env;

                const std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

                if (desc->err && desc->err->sqlstate && desc->err->error_msg) {
                    err = new ERR_INFO(*desc->err);
                } else if (desc->wrapped_desc) {
                    has_underlying_data = true;
                    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
                        HandleType, desc->wrapped_desc, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr
                    );
                    ret = RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
                }
            }
            break;
        default:
            return SQL_INVALID_HANDLE;
    }

    if (err) {
        // No support for multi-error / list
        //  at most 1 for our Wrapper Layer
        if (RecNumber != 1) {
            delete err;
            return SQL_NO_DATA_FOUND;
        }

        ret = SQL_SUCCESS;
        if (SQLState) {
#ifdef UNICODE
            CopyUTF8ToUTF16Buffer(reinterpret_cast<uint16_t*>(SQLState), MAX_SQL_STATE_LEN, err->sqlstate);
#else
            snprintf(reinterpret_cast<RDS_CHAR*>(SQLState), MAX_SQL_STATE_LEN, RDS_CHAR_FORMAT, AS_RDS_CHAR(err->sqlstate));
#endif
        }
        const SQLLEN err_len = err->error_msg != nullptr ? static_cast<SQLLEN>(strlen(err->error_msg)) : 0;
        if (TextLengthPtr) {
            *TextLengthPtr = static_cast<SQLSMALLINT>(err_len * sizeof(SQLTCHAR));
            if (BufferLength == 0) {
                ret = SQL_SUCCESS_WITH_INFO;
            } else if (err_len >= BufferLength) {
                *TextLengthPtr = static_cast<SQLSMALLINT>((err_len - 1) * sizeof(SQLTCHAR));
            }
        }
        if (MessageText && (BufferLength > 0) && err->error_msg) {
#ifdef UNICODE
            const SQLLEN written = CopyUTF8ToUTF16Buffer(reinterpret_cast<uint16_t*>(MessageText), static_cast<size_t>(BufferLength) / sizeof(uint16_t), err->error_msg);
#else
            const SQLLEN written = snprintf(reinterpret_cast<RDS_CHAR*>(MessageText), static_cast<size_t>(BufferLength) / sizeof(SQLTCHAR), RDS_CHAR_FORMAT, AS_RDS_CHAR(err->error_msg));
#endif
            if (written >= BufferLength) {
                ret = SQL_SUCCESS_WITH_INFO;
            }
            if (TextLengthPtr) {
                *TextLengthPtr = static_cast<SQLSMALLINT>(written * sizeof(SQLTCHAR));
            }
        }
        if (NativeErrorPtr) {
            *NativeErrorPtr = static_cast<SQLINTEGER>(err->native_err);
        }
        delete err;
    } else if (!has_underlying_data) {
        // No Data from wrapper or underlying driver
        //  Set states and clear buffers
        if (SQLState) {
#ifdef UNICODE
            CopyUTF8ToUTF16Buffer(reinterpret_cast<uint16_t*>(SQLState), MAX_SQL_STATE_LEN, NO_DATA_SQL_STATE);
#else
            snprintf(reinterpret_cast<RDS_CHAR*>(SQLState), MAX_SQL_STATE_LEN, RDS_CHAR_FORMAT, NO_DATA_SQL_STATE);
#endif
        }
        if (MessageText) {
            MessageText[0] = '\0';
        }
        if (NativeErrorPtr) {
            *NativeErrorPtr = static_cast<SQLINTEGER>(NO_DATA_NATIVE_ERR);
        }
        if (TextLengthPtr) {
            *TextLengthPtr = static_cast<SQLSMALLINT>(NO_DATA_NATIVE_ERR);
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
    SQLULEN len = sizeof(SQLSMALLINT);
    SQLULEN value = 0;
    const RDS_CHAR* char_value = nullptr;
    RDS_CHAR odbcver[ODBC_VER_SiZE];

    // Query underlying driver if connection is established
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    if (dbc == nullptr) {
        return SQL_INVALID_HANDLE;
    }

    // Naming and version for Wrapper
    switch (InfoType) {
        case SQL_DRIVER_NAME:
            char_value = DRIVER_NAME;
            break;
        case SQL_DRIVER_VER:
            char_value = DRIVER_VERSION;
            break;
        default:
            break;
    }

    if (!char_value) {
        {
            const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
            if (dbc->wrapped_dbc) {
                const ENV* env = dbc->env;
                CLEAR_DBC_ERROR(dbc);
                const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetInfo, RDS_STR_SQLGetInfo,
                    dbc->wrapped_dbc, InfoType, InfoValuePtr, BufferLength, StringLengthPtr
                );
                return RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
            }
        }

        // Get info for shell driver
        switch (InfoType) {
            case SQL_DRIVER_ODBC_VER:
                snprintf(odbcver, ODBC_VER_SiZE, "%02x.%02x", ODBCVER / ODBCVER_BITS, ODBCVER % ODBCVER_BITS);
                char_value = odbcver;
                break;
            case SQL_MAX_CONCURRENT_ACTIVITIES:
                value = 0; // No Limit
                break;
            case SQL_ASYNC_DBC_FUNCTIONS:
            case SQL_ASYNC_NOTIFICATION:
                value = 1; // "Supported"
                break;
            // TODO - Add other cases as needed
            default:
                const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
                LOG(ERROR) << "[" << InfoType << "] not implemented for AWS Advanced ODBC Wrapper's SQLGetInfo";
                CLEAR_DBC_ERROR(dbc);
                dbc->err = new ERR_INFO("SQLGetInfo - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
                NOT_IMPLEMENTED;
        }
    }
    ret = SQL_SUCCESS;

    // Pass info back to caller
    if (char_value) {
        len = RDS_STR_LEN(char_value);
        if (InfoValuePtr) {
#ifdef UNICODE
            CopyUTF8ToUTF16Buffer(reinterpret_cast<uint16_t*>(InfoValuePtr), BufferLength / sizeof(uint16_t), char_value);
#else
            snprintf(static_cast<RDS_CHAR*>(InfoValuePtr), static_cast<size_t>(BufferLength) / sizeof(SQLTCHAR), RDS_CHAR_FORMAT, char_value);
#endif
            if (len >= BufferLength) {
                ret = SQL_SUCCESS_WITH_INFO;
            }
        }
        if (StringLengthPtr) {
            *StringLengthPtr = static_cast<SQLSMALLINT>(len * sizeof(SQLTCHAR));
        }
    } else {
        if (InfoValuePtr) {
            if (len == sizeof(SQLSMALLINT)) {
                *(static_cast<SQLUSMALLINT*>(InfoValuePtr)) = static_cast<SQLUSMALLINT>(value);
            }
            else if (len == sizeof(SQLINTEGER)) {
                *(static_cast<SQLUINTEGER*>(InfoValuePtr)) = static_cast<SQLUINTEGER>(value);
            }
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    RdsLibResult res;
    switch (Attribute) {
        case SQL_ATTR_APP_ROW_DESC:
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetStmtAttr, RDS_STR_SQLGetStmtAttr,
                stmt->wrapped_stmt, Attribute, &(stmt->app_row_desc->wrapped_desc), BufferLength, StringLengthPtr
            );
            *(static_cast<SQLPOINTER*>(ValuePtr)) = stmt->app_row_desc;
            return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
        case SQL_ATTR_APP_PARAM_DESC:
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetStmtAttr, RDS_STR_SQLGetStmtAttr,
                stmt->wrapped_stmt, Attribute, &(stmt->app_param_desc->wrapped_desc), BufferLength, StringLengthPtr
            );
            *(static_cast<SQLPOINTER*>(ValuePtr)) = stmt->app_param_desc;
            return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
        case SQL_ATTR_IMP_ROW_DESC:
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetStmtAttr, RDS_STR_SQLGetStmtAttr,
                stmt->wrapped_stmt, Attribute, &(stmt->imp_row_desc->wrapped_desc), BufferLength, StringLengthPtr
            );
            *(static_cast<SQLPOINTER*>(ValuePtr)) = stmt->imp_row_desc;
            return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
        case SQL_ATTR_IMP_PARAM_DESC:
            res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetStmtAttr, RDS_STR_SQLGetStmtAttr,
                stmt->wrapped_stmt, Attribute, &(stmt->imp_param_desc->wrapped_desc), BufferLength, StringLengthPtr
            );
            *(static_cast<SQLPOINTER*>(ValuePtr)) = stmt->imp_param_desc;
            return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
        default:
            break;
    }

    CHECK_WRAPPED_STMT(stmt);
    res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetStmtAttr, RDS_STR_SQLGetStmtAttr,
        stmt->wrapped_stmt, Attribute, ValuePtr, BufferLength, StringLengthPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLGetTypeInfo(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    DataType)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetTypeInfo, RDS_STR_SQLGetTypeInfo,
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
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
    CLEAR_DBC_ERROR(dbc);

    CHECK_WRAPPED_DBC(dbc);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLNativeSql, RDS_STR_SQLNativeSql,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLPrepare, RDS_STR_SQLPrepare,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLPrimaryKeys, RDS_STR_SQLPrimaryKeys,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLProcedureColumns, RDS_STR_SQLProcedureColumns,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLProcedures, RDS_STR_SQLProcedures,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, ProcName, NameLength3
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_SQLSetConnectOption(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   Option,
    SQLULEN        Param)
{
    SQLINTEGER value_len = 0;
    if (Option == SQL_ATTR_CURRENT_CATALOG) {
        value_len = SQL_NTS;
    }
    return RDS_SQLSetConnectAttr(ConnectionHandle, Option, reinterpret_cast<SQLPOINTER>(Param), value_len);
}

SQLRETURN RDS_SQLSetCursorName(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CursorName,
    SQLSMALLINT    NameLength)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;
    SQLRETURN ret = SQL_SUCCESS;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    if (stmt->wrapped_stmt) {
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetCursorName, RDS_STR_SQLSetCursorName,
            stmt->wrapped_stmt, CursorName, NameLength
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
    }

    std::string conn_str_utf8;
#ifdef UNICODE
    conn_str_utf8 = ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(CursorName));
#else
    conn_str_utf8 = reinterpret_cast<const char *>(CursorName);
#endif
    const size_t load_len = NameLength == SQL_NTS ? conn_str_utf8.length() : NameLength;
    stmt->cursor_name = conn_str_utf8.substr(0, load_len);
    return ret;
}

SQLRETURN RDS_SQLSetDescField(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength)
{
    NULL_CHECK_ENV_ACCESS_DESC(DescriptorHandle);
    DESC* desc = static_cast<DESC*>(DescriptorHandle);
    const DBC* dbc = desc->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

    CHECK_WRAPPED_DESC(desc);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetDescField, RDS_STR_SQLSetDescField,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;
    SQLRETURN ret = SQL_SUCCESS;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    switch (Attribute) {
        // Read-only attributes
        case SQL_ATTR_IMP_PARAM_DESC:
        case SQL_ATTR_IMP_ROW_DESC:
        case SQL_ATTR_ROW_NUMBER:
            stmt->err = new ERR_INFO("Attribute is read-only for Statement Handles", ERR_INVALID_ATTRIBUTE_VALUE);
            return SQL_ERROR;
        default:
            break;
    }

    if (stmt->wrapped_stmt) {
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetStmtAttr, RDS_STR_SQLSetStmtAttr,
            stmt->wrapped_stmt, Attribute, ValuePtr, StringLength
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
    }
    stmt->attr_map.insert_or_assign(Attribute, std::make_pair(ValuePtr, StringLength));

    return ret;
}

SQLRETURN RDS_SQLSpecialColumns(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   IdentifierType,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLUSMALLINT   Scope,
    SQLUSMALLINT   Nullable)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSpecialColumns, RDS_STR_SQLSpecialColumns,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLStatistics, RDS_STR_SQLStatistics,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLTablePrivileges, RDS_STR_SQLTablePrivileges,
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
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLTables, RDS_STR_SQLTables,
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, TableType, NameLength4
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN RDS_InitializeConnection(DBC* dbc)
{
    ENV* env = dbc->env;

    // Remove input Driver
    // We don't want the underlying connection
    //  to look back to the wrapper
    // Also allows the Base DSN parse driver into map
    dbc->conn_attr.erase(KEY_DRIVER);

    // Set the DSN use the Base if one is found
    if (dbc->conn_attr.contains(KEY_BASE_DSN)) {
        dbc->conn_attr.insert_or_assign(KEY_DSN, dbc->conn_attr.at(KEY_BASE_DSN));
        // Load Base DSN info, should contain driver to use
        OdbcDsnHelper::LoadAll(dbc->conn_attr.at(KEY_BASE_DSN), dbc->conn_attr);
    }

    // If driver is not loaded from Base DSN, try input base driver
    if (!dbc->conn_attr.contains(KEY_DRIVER) && dbc->conn_attr.contains(KEY_BASE_DRIVER)) {
        dbc->conn_attr.insert_or_assign(KEY_DRIVER, dbc->conn_attr.at(KEY_BASE_DRIVER));
    }

    const std::unordered_set<RDS_STR> invalid_params = AttributeValidator::ValidateMap(dbc->conn_attr);
    if (!invalid_params.empty()) {
        std::string invalid_message("Invalid value specified for connection string attribute:\n\t");
        for (const RDS_STR& msg : invalid_params) {
            invalid_message += msg;
            invalid_message += "\n\t";
        }
        LOG(ERROR) << invalid_message;
        CLEAR_DBC_ERROR(dbc);
        dbc->err = new ERR_INFO(invalid_message.c_str(), WARN_INVALID_CONNECTION_STRING_ATTRIBUTE);
        return SQL_ERROR;
    }

    if (dbc->conn_attr.contains(KEY_DRIVER)) {
        // TODO - Need to ensure the paths (slashes) are correct per OS
        const RDS_STR driver_path = dbc->conn_attr.at(KEY_DRIVER);

        // Load Module to Env if empty
        if (!env->driver_lib_loader) {
            env->driver_lib_loader = std::make_shared<RdsLibLoader>(driver_path);
        } else if (driver_path != env->driver_lib_loader->GetDriverPath()) {
            LOG(ERROR) << "Environment underlying driver differs from new connect. Create a new environment for different underlying drivers";
            CLEAR_DBC_ERROR(dbc);
            dbc->err = new ERR_INFO("Environment underlying driver differs from new connect. Create a new environment for different underlying drivers", ERR_DIFF_ENV_UNDERLYING_DRIVER);
            return SQL_ERROR;
        }
    } else {
        LOG(ERROR) << "No underlying driver found. Provide proper path to [BASE_DRIVER] or [DRIVER] within the [BASE_DSN]";
        CLEAR_DBC_ERROR(dbc);
        dbc->err = new ERR_INFO("No underlying driver found. Provide proper path to [BASE_DRIVER] or [DRIVER] within the [BASE_DSN]", ERR_NO_UNDER_LYING_DRIVER);
        return SQL_ERROR;
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
            ret = RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res) == SQL_SUCCESS
                ? ret : static_cast<SQLRETURN>(SQL_SUCCESS_WITH_INFO);
        }
    }

    // Initialize Plugins
    try {
        if (!dbc->plugin_head) {
            BasePlugin* plugin_head = new BasePlugin(dbc);
            BasePlugin* next_plugin;

            // Auth Plugins
            if (dbc->conn_attr.contains(KEY_AUTH_TYPE)) {
                const AuthType type = AuthProvider::AuthTypeFromString(dbc->conn_attr.at(KEY_AUTH_TYPE));
                switch (type) {
                        case AuthType::IAM:
                            next_plugin = new IamAuthPlugin(dbc, plugin_head);
                            plugin_head = next_plugin;
                            break;
                        case AuthType::SECRETS_MANAGER:
                            next_plugin = new SecretsManagerPlugin(dbc, plugin_head);
                            plugin_head = next_plugin;
                            break;
                        case AuthType::ADFS:
                            next_plugin = new AdfsAuthPlugin(dbc, plugin_head);
                            plugin_head = next_plugin;
                            break;
                        case AuthType::OKTA:
                            next_plugin = new OktaAuthPlugin(dbc, plugin_head);
                            plugin_head = next_plugin;
                            break;
                        case AuthType::DATABASE:
                        case AuthType::INVALID:
                        default:
                            break;
                }
            }

            // Limitless
            if (dbc->conn_attr.contains(KEY_ENABLE_LIMITLESS)
                && dbc->conn_attr.at(KEY_ENABLE_LIMITLESS) == VALUE_BOOL_TRUE)
            {
                next_plugin = new LimitlessPlugin(dbc, plugin_head);
                plugin_head = next_plugin;
            }

            // Failover
            if (dbc->conn_attr.contains(KEY_ENABLE_FAILOVER)
                && dbc->conn_attr.at(KEY_ENABLE_FAILOVER) == VALUE_BOOL_TRUE)
            {
                next_plugin = new FailoverPlugin(dbc, plugin_head);
                plugin_head = next_plugin;
            }

            // Finalize and track in DBC
            dbc->plugin_head = plugin_head;
        }
    } catch (const std::exception& ex) {
        const std::string err_msg = std::string("Error initializing plugins: ") + ex.what();
        LOG(ERROR) << err_msg;
        CLEAR_DBC_ERROR(dbc);
        dbc->err = new ERR_INFO(err_msg.c_str(), WARN_INVALID_CONNECTION_STRING_ATTRIBUTE);
        return SQL_ERROR;
    }

    return ret;
}
