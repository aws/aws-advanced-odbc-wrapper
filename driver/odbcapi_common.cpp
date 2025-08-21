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

#include "odbcapi.h"

#include "plugin/iam/iam_auth_plugin.h"

#include "util/connection_string_helper.h"
#include "util/connection_string_keys.h"
#include "util/odbc_dsn_helper.h"
#include "util/rds_lib_loader.h"
#include "util/rds_strings.h"

#include "driver.h"
#include "odbcapi_rds_helper.h"

#include <cstring>

// Common ODBC Functions
SQLRETURN SQL_API SQLAllocConnect(
    SQLHENV        EnvironmentHandle,
    SQLHDBC *      ConnectionHandle)
{
    return RDS_AllocDbc(EnvironmentHandle, ConnectionHandle);
};

SQLRETURN SQL_API SQLAllocEnv(
    SQLHENV *      EnvironmentHandle)
{
    return RDS_AllocEnv(EnvironmentHandle);
}

SQLRETURN SQL_API SQLAllocHandle(
    SQLSMALLINT    HandleType,
    SQLHANDLE      InputHandle,
    SQLHANDLE *    OutputHandlePtr)
{
    SQLRETURN ret = SQL_ERROR;
    switch (HandleType) {
        case SQL_HANDLE_ENV:
            ret = RDS_AllocEnv(OutputHandlePtr);
            break;
        case SQL_HANDLE_DBC:
            ret = RDS_AllocDbc(InputHandle, OutputHandlePtr);
            break;
        case SQL_HANDLE_STMT:
            ret = RDS_AllocStmt(InputHandle, OutputHandlePtr);
            break;
        case SQL_HANDLE_DESC:
            ret = RDS_AllocDesc(InputHandle, OutputHandlePtr);
            break;
        default:
            ret = SQL_INVALID_HANDLE;
    }
    return ret;
}

SQLRETURN SQL_API SQLAllocStmt(
    SQLHDBC        ConnectionHandle,
    SQLHSTMT *     StatementHandle)
{
    return RDS_AllocStmt(ConnectionHandle, StatementHandle);
}

SQLRETURN SQL_API SQLBindCol(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLSMALLINT    TargetType,
    SQLPOINTER     TargetValuePtr,
    SQLLEN         BufferLength,
    SQLLEN *       StrLen_or_IndPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLBindCol, RDS_STR_SQLBindCol,
        stmt->wrapped_stmt, ColumnNumber, TargetType, TargetValuePtr, BufferLength, StrLen_or_IndPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLBindParameter(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ParameterNumber,
    SQLSMALLINT    InputOutputType,
    SQLSMALLINT    ValueType,
    SQLSMALLINT    ParameterType,
    SQLULEN        ColumnSize,
    SQLSMALLINT    DecimalDigits,
    SQLPOINTER     ParameterValuePtr,
    SQLLEN         BufferLength,
    SQLLEN *       StrLen_or_IndPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLBindParameter, RDS_STR_SQLBindParameter,
        stmt->wrapped_stmt, ParameterNumber, InputOutputType, ValueType, ParameterType, ColumnSize, DecimalDigits, ParameterValuePtr, BufferLength, StrLen_or_IndPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLBulkOperations(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Operation)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLBulkOperations, RDS_STR_SQLBulkOperations,
        stmt->wrapped_stmt, Operation
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLCancel(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLCancel, RDS_STR_SQLCancel,
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLCancelHandle(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle)
{
    DESC *desc;
    STMT *stmt;
    DBC *dbc;
    ENV *env;
    RdsLibResult res;
    SQLRETURN ret = SQL_ERROR;
    switch (HandleType) {
        case SQL_HANDLE_STMT:
            NULL_CHECK_ENV_ACCESS_STMT(Handle);
            {
                stmt = (STMT*) Handle;
                dbc = (DBC*) stmt->dbc;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

                CHECK_WRAPPED_STMT(stmt);
                res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLCancel, RDS_STR_SQLCancel,
                    stmt->wrapped_stmt
                );
                ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
            }
            break;
        case SQL_HANDLE_ENV:
        {
            NULL_CHECK_HANDLE(Handle);
            env = (ENV*) Handle;
            std::lock_guard<std::recursive_mutex> lock_guard(env->lock);
            CLEAR_ENV_ERROR(env);
            env->err = new ERR_INFO("SQLCancelHandle - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
            NOT_IMPLEMENTED;
        }
        case SQL_HANDLE_DBC:
        {
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            dbc = (DBC*) Handle;
            std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
            CLEAR_DBC_ERROR(dbc);
            dbc->err = new ERR_INFO("SQLCancelHandle - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
            NOT_IMPLEMENTED;
        }
        case SQL_HANDLE_DESC:
        {
            NULL_CHECK_ENV_ACCESS_DESC(Handle);
            desc = (DESC*) Handle;
            std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);
            CLEAR_DESC_ERROR(desc);
            desc->err = new ERR_INFO("SQLCancelHandle - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
            NOT_IMPLEMENTED;
        }
        default:
            // TODO - Set Error
            ret = SQL_INVALID_HANDLE;
            break;
    }

    return ret;
}

SQLRETURN SQL_API SQLCloseCursor(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;
    SQLRETURN ret = SQL_SUCCESS;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    if (stmt->wrapped_stmt) {
        RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLCloseCursor, RDS_STR_SQLCloseCursor,
            stmt->wrapped_stmt
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
    }

    return ret;
}

// TODO Maybe - Impl SQLCompleteAsync
// Both PostgreSQL AND MySQL do NOT implement this
SQLRETURN SQL_API SQLCompleteAsync(
    SQLSMALLINT   HandleType,
    SQLHANDLE     Handle,
    RETCODE *     AsyncRetCodePtr)
{
    switch (HandleType) {
        case SQL_HANDLE_DBC:
        {
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            DBC *dbc = (DBC*) Handle;
            std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
            CLEAR_DBC_ERROR(dbc);
            dbc->err = new ERR_INFO("SQLCompleteAsync - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
            break;
        }

        case SQL_HANDLE_STMT:
        {
            NULL_CHECK_ENV_ACCESS_STMT(Handle);
            STMT *stmt = (STMT*) Handle;
            std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
            CLEAR_DBC_ERROR(stmt);
            stmt->err = new ERR_INFO("SQLCompleteAsync - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
            break;
        }
        default:
            return SQL_INVALID_HANDLE;
    }
    NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLCopyDesc(
    SQLHDESC       SourceDescHandle,
    SQLHDESC       TargetDescHandle)
{
    NULL_CHECK_ENV_ACCESS_DESC(SourceDescHandle);
    DESC* src_desc = (DESC*) SourceDescHandle;
    DESC* dst_desc = TargetDescHandle ? (DESC*) TargetDescHandle : new DESC();
    DBC* src_dbc = (DBC*) src_desc->dbc;
    ENV* src_env = (ENV*) src_dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard_src(src_desc->lock);
    std::lock_guard<std::recursive_mutex> lock_guard_dst(dst_desc->lock);

    CHECK_WRAPPED_DESC(src_desc);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(src_env->driver_lib_loader, RDS_FP_SQLCopyDesc, RDS_STR_SQLCopyDesc,
        src_desc->wrapped_desc, dst_desc->wrapped_desc
    );
    SQLRETURN ret = RDS_ProcessLibRes(SQL_HANDLE_DESC, dst_desc, res);

    // Move to use new DBC
    if (dst_desc->dbc) {
        dst_desc->dbc->desc_list.remove(dst_desc);
    }
    src_dbc->desc_list.emplace_back(dst_desc);
    dst_desc->dbc = src_desc->dbc;

    return ret;
}

SQLRETURN SQL_API SQLDescribeParam(
    SQLHSTMT      StatementHandle,
    SQLUSMALLINT  ParameterNumber,
    SQLSMALLINT * DataTypePtr,
    SQLULEN *     ParameterSizePtr,
    SQLSMALLINT * DecimalDigitsPtr,
    SQLSMALLINT * NullablePtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDescribeParam, RDS_STR_SQLDescribeParam,
        stmt->wrapped_stmt, ParameterNumber, DataTypePtr, ParameterSizePtr, DecimalDigitsPtr, NullablePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLDisconnect(
    SQLHDBC        ConnectionHandle)
{
    SQLRETURN ret = SQL_ERROR;
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
    CLEAR_DBC_ERROR(dbc);

    CHECK_WRAPPED_DBC(dbc);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
        dbc->wrapped_dbc
    );
    ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
    if (SQL_SUCCEEDED(ret)) {
        dbc->conn_status = CONN_NOT_CONNECTED;
    }
    return ret;
}

SQLRETURN SQL_API SQLEndTran(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    CompletionType)
{
    return RDS_SQLEndTran(HandleType, Handle, CompletionType);
}

SQLRETURN SQL_API SQLExecute(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    if (dbc->plugin_head) {
        return dbc->plugin_head->Execute(StatementHandle);
    }

    stmt->err = new ERR_INFO("SQLExecute - Connection not open", ERR_CONNECTION_NOT_OPEN);
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLExtendedFetch(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   FetchOrientation,
    SQLLEN         FetchOffset,
    SQLULEN *      RowCountPtr,
    SQLUSMALLINT * RowStatusArray)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLExtendedFetch, RDS_STR_SQLExtendedFetch,
        stmt->wrapped_stmt, FetchOrientation, FetchOffset, RowCountPtr, RowStatusArray
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLFetch(
    SQLHSTMT        StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFetch, RDS_STR_SQLFetch,
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLFetchScroll(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    FetchOrientation,
    SQLLEN         FetchOffset)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFetchScroll, RDS_STR_SQLFetchScroll,
        stmt->wrapped_stmt, FetchOrientation, FetchOffset
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLFreeConnect(
    SQLHDBC        ConnectionHandle)
{
    return RDS_FreeConnect(ConnectionHandle);
}

SQLRETURN SQL_API SQLFreeEnv(
    SQLHENV        EnvironmentHandle)
{
    return RDS_FreeEnv(EnvironmentHandle);
}

SQLRETURN SQL_API SQLFreeHandle(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle)
{
    SQLRETURN ret = SQL_ERROR;
    switch (HandleType) {
        case SQL_HANDLE_DBC:
            ret = RDS_FreeConnect(Handle);
            break;
        case SQL_HANDLE_DESC:
            ret = RDS_FreeDesc(Handle);
            break;
        case SQL_HANDLE_ENV:
            ret = RDS_FreeEnv(Handle);
            break;
        case SQL_HANDLE_STMT:
            ret = RDS_FreeStmt(Handle);
            break;
        default:
            ret = SQL_ERROR;
    }

    return ret;
}

SQLRETURN SQL_API SQLFreeStmt(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Option)
{
    return RDS_FreeStmt(StatementHandle);
}

SQLRETURN SQL_API SQLGetData(
    SQLHSTMT      StatementHandle,
    SQLUSMALLINT  Col_or_Param_Num,
    SQLSMALLINT   TargetType,
    SQLPOINTER    TargetValuePtr,
    SQLLEN        BufferLength,
    SQLLEN *      StrLen_or_IndPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetData, RDS_STR_SQLGetData,
        stmt->wrapped_stmt, Col_or_Param_Num, TargetType, TargetValuePtr, BufferLength, StrLen_or_IndPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLGetEnvAttr(
    SQLHENV        EnvironmentHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV *env = (ENV*) EnvironmentHandle;

    std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

    if (env->attr_map.contains(Attribute)) {
        std::pair<SQLPOINTER, SQLINTEGER> value_pair = env->attr_map.at(Attribute);
        if (value_pair.second == sizeof(SQLSMALLINT)) {
            *((SQLUSMALLINT *) ValuePtr) = static_cast<SQLUSMALLINT>(reinterpret_cast<uintptr_t>(value_pair.first));
        } else if (value_pair.second == sizeof(SQLUINTEGER) || value_pair.second == 0) {
            *((SQLUINTEGER *) ValuePtr) = static_cast<SQLUINTEGER>(reinterpret_cast<uintptr_t>(value_pair.first));
        } else {
            snprintf((char *) ValuePtr, (size_t) BufferLength, "%s", (char *) value_pair.first);
            if (value_pair.second >= BufferLength) {
                return SQL_SUCCESS_WITH_INFO;
            }
        }
        return SQL_SUCCESS;
    }
    return SQL_ERROR;
}

/*
    TODO - NOT YET IMPLEMENTED
    HARDCODED TO RETURN TRUE / ASSUMES ALL FUNCTIONS IMPLEMENTED
*/
// TODO - Impl SQLGetFunctions
SQLRETURN SQL_API SQLGetFunctions(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   FunctionId,
    SQLUSMALLINT * SupportedPtr)
{
    DBC *dbc = (DBC*) ConnectionHandle;
    SQLRETURN ret = SQL_ERROR;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
    CLEAR_DBC_ERROR(dbc);

    // Query underlying driver if connection is established
    if (dbc && dbc->wrapped_dbc) {
        ENV* env = (ENV*) dbc->env;
        RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetFunctions, RDS_STR_SQLGetFunctions,
            dbc->wrapped_dbc, FunctionId, SupportedPtr
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
    } else {
        // TODO - THIS IS HARDCODED
        // Will need to keep track of a map of the current implemented functions
        // as ODBC adds/removes functions in the future
        *SupportedPtr = SQL_TRUE;
        ret = SQL_SUCCESS;
    }

    return ret;
}

SQLRETURN SQL_API SQLGetStmtOption(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetStmtOption, RDS_STR_SQLGetStmtOption,
        stmt->wrapped_stmt, Attribute, ValuePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLMoreResults(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLMoreResults, RDS_STR_SQLMoreResults,
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLNumParams(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT *  ParameterCountPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLNumParams, RDS_STR_SQLNumParams,
        stmt->wrapped_stmt, ParameterCountPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLNumResultCols(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT *  ColumnCountPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLNumResultCols, RDS_STR_SQLNumResultCols,
        stmt->wrapped_stmt, ColumnCountPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLParamData(
    SQLHSTMT       StatementHandle,
    SQLPOINTER *   ValuePtrPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLParamData, RDS_STR_SQLParamData,
        stmt->wrapped_stmt, ValuePtrPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLParamOptions(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Crow,
    SQLINTEGER *   FetchOffsetPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLParamOptions, RDS_STR_SQLParamOptions,
        stmt->wrapped_stmt, Crow, FetchOffsetPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLPutData(
    SQLHSTMT       StatementHandle,
    SQLPOINTER     DataPtr,
    SQLLEN         StrLen_or_Ind)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLPutData, RDS_STR_SQLPutData,
        stmt->wrapped_stmt, DataPtr, StrLen_or_Ind
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLRowCount(
    SQLHSTMT       StatementHandle,
    SQLLEN *       RowCountPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLRowCount, RDS_STR_SQLRowCount,
        stmt->wrapped_stmt, RowCountPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLSetDescRec(
    SQLHDESC      DescriptorHandle,
    SQLSMALLINT   RecNumber,
    SQLSMALLINT   Type,
    SQLSMALLINT   SubType,
    SQLLEN        Length,
    SQLSMALLINT   Precision,
    SQLSMALLINT   Scale,
    SQLPOINTER    DataPtr,
    SQLLEN *      StringLengthPtr,
    SQLLEN *      IndicatorPtr)
{
    NULL_CHECK_ENV_ACCESS_DESC(DescriptorHandle);
    DESC *desc = (DESC*) DescriptorHandle;
    DBC *dbc = (DBC*) desc->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

    CHECK_WRAPPED_DESC(desc);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetDescRec, RDS_STR_SQLSetDescRec,
        desc->wrapped_desc, RecNumber, Type, SubType, Length, Precision, Scale, DataPtr, StringLengthPtr, IndicatorPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
}

SQLRETURN SQL_API SQLSetEnvAttr(
    SQLHENV        EnvironmentHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV *env = (ENV*) EnvironmentHandle;
    SQLRETURN ret = SQL_SUCCESS;

    std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

    // Track new value
    env->attr_map.insert_or_assign(Attribute, std::make_pair(ValuePtr, StringLength));

    // Check if underlying library is loaded
    //  Don't fail if it isn't loaded as
    //  this can be called prior to connecting
    if (env->driver_lib_loader && env->wrapped_env) {
        // Update existing connections environments
        RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetEnvAttr, RDS_STR_SQLSetEnvAttr,
            env->wrapped_env, Attribute, ValuePtr, StringLength
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
    }

    return ret;
}

SQLRETURN SQL_API SQLSetParam(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ParameterNumber,
    SQLSMALLINT    ValueType,
    SQLSMALLINT    ParameterType,
    SQLULEN        ColumnSize,
    SQLSMALLINT    DecimalDigits,
    SQLPOINTER     ParameterValuePtr,
    SQLLEN *       StrLen_or_IndPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetParam, RDS_STR_SQLSetParam,
        stmt->wrapped_stmt, ParameterNumber, ValueType, ParameterType, ColumnSize, DecimalDigits, ParameterValuePtr, StrLen_or_IndPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLSetPos(
    SQLHSTMT       StatementHandle,
    SQLSETPOSIROW  RowNumber,
    SQLUSMALLINT   Operation,
    SQLUSMALLINT   LockType)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetPos, RDS_STR_SQLSetPos,
        stmt->wrapped_stmt, RowNumber, Operation, LockType
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLSetScrollOptions(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Concurrency,
    SQLLEN         KeysetSize,
    SQLUSMALLINT   RowsetSize)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetScrollOptions, RDS_STR_SQLSetScrollOptions,
        stmt->wrapped_stmt, Concurrency, KeysetSize, RowsetSize
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLSetStmtOption(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Option,
    SQLULEN        Param)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetStmtOption, RDS_STR_SQLSetStmtOption,
        stmt->wrapped_stmt, Option, Param
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLTransact(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   CompletionType)
{
    if (nullptr == EnvironmentHandle && nullptr == ConnectionHandle) {
        return SQL_INVALID_HANDLE;
    }

    return RDS_SQLEndTran(
        ConnectionHandle ? SQL_HANDLE_DBC : SQL_HANDLE_ENV,
        ConnectionHandle ? ConnectionHandle : EnvironmentHandle,
        CompletionType
    );
}
