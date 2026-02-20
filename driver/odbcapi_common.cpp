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

#include "error.h"
#include "odbcapi_rds_helper.h"
#include "plugin/base_plugin.h"
#include "util/rds_lib_loader.h"

// Common ODBC Functions
SQLRETURN SQL_API SQLAllocConnect(
    SQLHENV        EnvironmentHandle,
    SQLHDBC *      ConnectionHandle)
{
    LOG(INFO) << "Entering SQLAllocConnect";
    return RDS_AllocDbc(EnvironmentHandle, ConnectionHandle);
};

SQLRETURN SQL_API SQLAllocEnv(
    SQLHENV *      EnvironmentHandle)
{
    LOG(INFO) << "Entering SQLAllocEnv";
    return RDS_AllocEnv(EnvironmentHandle);
}

SQLRETURN SQL_API SQLAllocHandle(
    SQLSMALLINT    HandleType,
    SQLHANDLE      InputHandle,
    SQLHANDLE *    OutputHandlePtr)
{
    LOG(INFO) << "Entering SQLAllocHandle";
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
    LOG(INFO) << "Entering SQLAllocStmt";
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
    LOG(INFO) << "Entering SQLBindCol";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLBindCol, RDS_STR_SQLBindCol,
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
    LOG(INFO) << "Entering SQLBindParameter";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLBindParameter, RDS_STR_SQLBindParameter,
        stmt->wrapped_stmt, ParameterNumber, InputOutputType, ValueType, ParameterType, ColumnSize, DecimalDigits, ParameterValuePtr, BufferLength, StrLen_or_IndPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLBulkOperations(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    Operation)
{
    LOG(INFO) << "Entering SQLBulkOperations";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLBulkOperations, RDS_STR_SQLBulkOperations,
        stmt->wrapped_stmt, Operation
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLCancel(
    SQLHSTMT       StatementHandle)
{
    LOG(INFO) << "Entering SQLCancel";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLCancel, RDS_STR_SQLCancel,
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLCancelHandle(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle)
{
    LOG(INFO) << "Entering SQLCancelHandle";
    DESC* desc;
    STMT* stmt;
    DBC* dbc;
    ENV* env;
    RdsLibResult res;
    SQLRETURN ret = SQL_ERROR;
    switch (HandleType) {
        case SQL_HANDLE_STMT:
            NULL_CHECK_ENV_ACCESS_STMT(Handle);
            {
                stmt = static_cast<STMT*>(Handle);
                dbc = stmt->dbc;
                env = dbc->env;

                const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

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
            env = static_cast<ENV*>(Handle);
            const std::lock_guard<std::recursive_mutex> lock_guard(env->lock);
            LOG(ERROR) << "Unsupported SQL API - SQLCancelHandle ENV";
            CLEAR_ENV_ERROR(env);
            env->err = new ERR_INFO("SQLCancelHandle ENV - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
            NOT_IMPLEMENTED;
        }
        case SQL_HANDLE_DBC:
        {
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            dbc = static_cast<DBC*>(Handle);
            const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
            LOG(ERROR) << "Unsupported SQL API - SQLCancelHandle DBC";
            CLEAR_DBC_ERROR(dbc);
            dbc->err = new ERR_INFO("SQLCancelHandle DBC - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
            NOT_IMPLEMENTED;
        }
        case SQL_HANDLE_DESC:
        {
            NULL_CHECK_ENV_ACCESS_DESC(Handle);
            desc = static_cast<DESC*>(Handle);
            const std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);
            LOG(ERROR) << "Unsupported SQL API - SQLCancelHandle DESC";
            CLEAR_DESC_ERROR(desc);
            desc->err = new ERR_INFO("SQLCancelHandle DESC - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
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
    LOG(INFO) << "Entering SQLCloseCursor";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;
    SQLRETURN ret = SQL_SUCCESS;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    if (stmt->wrapped_stmt) {
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLCloseCursor, RDS_STR_SQLCloseCursor,
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
    LOG(INFO) << "Entering SQLCompleteAsync";
    switch (HandleType) {
        case SQL_HANDLE_DBC:
        {
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            DBC* dbc = static_cast<DBC*>(Handle);
            const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
            LOG(ERROR) << "Unsupported SQL API - SQLCompleteAsync DBC";
            CLEAR_DBC_ERROR(dbc);
            dbc->err = new ERR_INFO("SQLCompleteAsync DBC - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
            break;
        }

        case SQL_HANDLE_STMT:
        {
            NULL_CHECK_ENV_ACCESS_STMT(Handle);
            STMT* stmt = static_cast<STMT*>(Handle);
            const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
            LOG(ERROR) << "Unsupported SQL API - SQLCompleteAsync STMT";
            CLEAR_DBC_ERROR(stmt);
            stmt->err = new ERR_INFO("SQLCompleteAsync STMT - API Unsupported", ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
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
    LOG(INFO) << "Entering SQLCopyDesc";
    NULL_CHECK_ENV_ACCESS_DESC(SourceDescHandle);
    DESC* src_desc = static_cast<DESC*>(SourceDescHandle);
    CHECK_WRAPPED_DESC(src_desc);

    DESC* dst_desc = TargetDescHandle ? static_cast<DESC*>(TargetDescHandle) : new DESC();
    DBC* src_dbc = src_desc->dbc;
    const ENV* src_env = src_dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard_src(src_desc->lock);
    const std::lock_guard<std::recursive_mutex> lock_guard_dst(dst_desc->lock);

    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(src_env->driver_lib_loader, RDS_FP_SQLCopyDesc, RDS_STR_SQLCopyDesc,
        src_desc->wrapped_desc, dst_desc->wrapped_desc
    );
    const SQLRETURN ret = RDS_ProcessLibRes(SQL_HANDLE_DESC, dst_desc, res);

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
    LOG(INFO) << "Entering SQLDescribeParam";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDescribeParam, RDS_STR_SQLDescribeParam,
        stmt->wrapped_stmt, ParameterNumber, DataTypePtr, ParameterSizePtr, DecimalDigitsPtr, NullablePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLDisconnect(
    SQLHDBC        ConnectionHandle)
{
    LOG(INFO) << "Entering SQLDisconnect";
    SQLRETURN ret = SQL_ERROR;
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
    CLEAR_DBC_ERROR(dbc);

    // Cleanup tracked statements
    const std::list<STMT*> stmt_list = dbc->stmt_list;
    for (STMT* stmt : stmt_list) {
        RDS_FreeStmt(stmt, SQL_DROP);
    }
    dbc->stmt_list.clear();

    CHECK_WRAPPED_DBC(dbc);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
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
    LOG(INFO) << "Entering SQLEndTran";
    return RDS_SQLEndTran(HandleType, Handle, CompletionType);
}

SQLRETURN SQL_API SQLExecute(
    SQLHSTMT       StatementHandle)
{
    LOG(INFO) << "Entering SQLExecute";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    DBC* dbc = stmt->dbc;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    if (dbc->plugin_head) {
        return dbc->plugin_head->Execute(StatementHandle);
    }

    LOG(ERROR) << "Cannot execute without an open connection";
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
    LOG(INFO) << "Entering SQLExtendedFetch";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLExtendedFetch, RDS_STR_SQLExtendedFetch,
        stmt->wrapped_stmt, FetchOrientation, FetchOffset, RowCountPtr, RowStatusArray
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLFetch(
    SQLHSTMT        StatementHandle)
{
    LOG(INFO) << "Entering SQLFetch";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFetch, RDS_STR_SQLFetch,
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLFetchScroll(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    FetchOrientation,
    SQLLEN         FetchOffset)
{
    LOG(INFO) << "Entering SQLFetchScroll";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFetchScroll, RDS_STR_SQLFetchScroll,
        stmt->wrapped_stmt, FetchOrientation, FetchOffset
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLFreeConnect(
    SQLHDBC        ConnectionHandle)
{
    LOG(INFO) << "Entering SQLFreeConnect";
    return RDS_FreeConnect(ConnectionHandle);
}

SQLRETURN SQL_API SQLFreeEnv(
    SQLHENV        EnvironmentHandle)
{
    LOG(INFO) << "Entering SQLFreeEnv";
    return RDS_FreeEnv(EnvironmentHandle);
}

SQLRETURN SQL_API SQLFreeHandle(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle)
{
    LOG(INFO) << "Entering SQLFreeHandle";
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
            ret = RDS_FreeStmt(Handle, SQL_DROP);
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
    LOG(INFO) << "Entering SQLFreeStmt";
    return RDS_FreeStmt(StatementHandle, Option);
}

SQLRETURN SQL_API SQLGetData(
    SQLHSTMT      StatementHandle,
    SQLUSMALLINT  Col_or_Param_Num,
    SQLSMALLINT   TargetType,
    SQLPOINTER    TargetValuePtr,
    SQLLEN        BufferLength,
    SQLLEN *      StrLen_or_IndPtr)
{
    LOG(INFO) << "Entering SQLGetData";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);

#if UNICODE
    RdsLibResult res;
    if (env->use_4_bytes && TargetType == SQL_C_TCHAR) {
        std::vector<SQLTCHAR> new_buf_vector(BufferLength*2, '\0');
        SQLTCHAR* new_buf = new_buf_vector.data();

        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetData, RDS_STR_SQLGetData,
            stmt->wrapped_stmt, Col_or_Param_Num, TargetType, new_buf, BufferLength, StrLen_or_IndPtr
        );
        SQLTCHAR* buf = reinterpret_cast<SQLTCHAR*>(TargetValuePtr);
        Convert4To2ByteString(env->use_4_bytes, new_buf, buf, BufferLength);
    } else {
        res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetData, RDS_STR_SQLGetData,
            stmt->wrapped_stmt, Col_or_Param_Num, TargetType, TargetValuePtr, BufferLength, StrLen_or_IndPtr
        );
    }
#else
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetData, RDS_STR_SQLGetData,
        stmt->wrapped_stmt, Col_or_Param_Num, TargetType, TargetValuePtr, BufferLength, StrLen_or_IndPtr
    );
#endif

    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLGetEnvAttr(
    SQLHENV        EnvironmentHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    LOG(INFO) << "Entering SQLGetEnvAttr";
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV* env = static_cast<ENV*>(EnvironmentHandle);

    const std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

    if (env->attr_map.contains(Attribute)) {
        const std::pair<SQLPOINTER, SQLINTEGER> value_pair = env->attr_map.at(Attribute);
        if (value_pair.second == sizeof(SQLSMALLINT)) {
            *(static_cast<SQLUSMALLINT*>(ValuePtr)) = static_cast<SQLUSMALLINT>(reinterpret_cast<uintptr_t>(value_pair.first));
        } else if (value_pair.second == sizeof(SQLUINTEGER) || value_pair.second == 0) {
            *(static_cast<SQLUINTEGER*>(ValuePtr)) = static_cast<SQLUINTEGER>(reinterpret_cast<uintptr_t>(value_pair.first));
        } else {
            snprintf(static_cast<char*>(ValuePtr), static_cast<size_t>(BufferLength), "%s", static_cast<char*>(value_pair.first));
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
    LOG(INFO) << "Entering SQLGetFunctions";
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    SQLRETURN ret = SQL_ERROR;

    if (dbc == nullptr) {
        return SQL_INVALID_HANDLE;
    }

    const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
    CLEAR_DBC_ERROR(dbc);

    // Query underlying driver if connection is established
    if (dbc->wrapped_dbc) {
        const ENV* env = dbc->env;
        const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetFunctions, RDS_STR_SQLGetFunctions,
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
    SQLUSMALLINT   Attribute,
    SQLPOINTER     ValuePtr)
{
    LOG(INFO) << "Entering SQLGetStmtOption";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetStmtOption, RDS_STR_SQLGetStmtOption,
        stmt->wrapped_stmt, Attribute, ValuePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLMoreResults(
    SQLHSTMT       StatementHandle)
{
    LOG(INFO) << "Entering SQLMoreResults";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLMoreResults, RDS_STR_SQLMoreResults,
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLNumParams(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT *  ParameterCountPtr)
{
    LOG(INFO) << "Entering SQLNumParams";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLNumParams, RDS_STR_SQLNumParams,
        stmt->wrapped_stmt, ParameterCountPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLNumResultCols(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT *  ColumnCountPtr)
{
    LOG(INFO) << "Entering SQLNumResultCols";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLNumResultCols, RDS_STR_SQLNumResultCols,
        stmt->wrapped_stmt, ColumnCountPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLParamData(
    SQLHSTMT       StatementHandle,
    SQLPOINTER *   ValuePtrPtr)
{
    LOG(INFO) << "Entering SQLParamData";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLParamData, RDS_STR_SQLParamData,
        stmt->wrapped_stmt, ValuePtrPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLParamOptions(
    SQLHSTMT       StatementHandle,
    SQLULEN        Crow,
    SQLULEN *      FetchOffsetPtr)
{
    LOG(INFO) << "Entering SQLParamOptions";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLParamOptions, RDS_STR_SQLParamOptions,
        stmt->wrapped_stmt, Crow, FetchOffsetPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLPutData(
    SQLHSTMT       StatementHandle,
    SQLPOINTER     DataPtr,
    SQLLEN         StrLen_or_Ind)
{
    LOG(INFO) << "Entering SQLPutData";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLPutData, RDS_STR_SQLPutData,
        stmt->wrapped_stmt, DataPtr, StrLen_or_Ind
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLRowCount(
    SQLHSTMT       StatementHandle,
    SQLLEN *       RowCountPtr)
{
    LOG(INFO) << "Entering SQLRowCount";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLRowCount, RDS_STR_SQLRowCount,
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
    LOG(INFO) << "Entering SQLSetDescRec";
    NULL_CHECK_ENV_ACCESS_DESC(DescriptorHandle);
    DESC* desc = static_cast<DESC*>(DescriptorHandle);
    const DBC* dbc = desc->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(desc->lock);

    CHECK_WRAPPED_DESC(desc);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetDescRec, RDS_STR_SQLSetDescRec,
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
    LOG(INFO) << "Entering SQLSetEnvAttr";
    return RDS_SQLSetEnvAttr(EnvironmentHandle, Attribute, ValuePtr, StringLength);
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
    LOG(INFO) << "Entering SQLSetParam";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetParam, RDS_STR_SQLSetParam,
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
    LOG(INFO) << "Entering SQLSetPos";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);
    CLEAR_STMT_ERROR(stmt);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetPos, RDS_STR_SQLSetPos,
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
    LOG(INFO) << "Entering SQLSetScrollOptions";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetScrollOptions, RDS_STR_SQLSetScrollOptions,
        stmt->wrapped_stmt, Concurrency, KeysetSize, RowsetSize
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLSetStmtOption(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Option,
    SQLULEN        Param)
{
    LOG(INFO) << "Entering SQLSetStmtOption";
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;
    const ENV* env = dbc->env;

    const std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    CHECK_WRAPPED_STMT(stmt);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLSetStmtOption, RDS_STR_SQLSetStmtOption,
        stmt->wrapped_stmt, Option, Param
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLTransact(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   CompletionType)
{
    LOG(INFO) << "Entering SQLTransact";
    if (nullptr == EnvironmentHandle && nullptr == ConnectionHandle) {
        return SQL_INVALID_HANDLE;
    }

    return RDS_SQLEndTran(
        ConnectionHandle ? SQL_HANDLE_DBC : SQL_HANDLE_ENV,
        ConnectionHandle ? ConnectionHandle : EnvironmentHandle,
        static_cast<SQLSMALLINT>(CompletionType)
    );
}
