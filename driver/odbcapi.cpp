#include "odbcapi.h"

#include "plugin/iam/iam_auth_plugin.h"

#include "util/auth_provider.h"
#include "util/connection_string_helper.h"
#include "util/connection_string_keys.h"
#include "util/rds_lib_loader.h"
#include "util/rds_strings.h"

#include "driver.h"

#include <cstring>

// RDS Functions
SQLRETURN RDS_AllocEnv_Impl(
    SQLHENV *      EnvironmentHandlePointer)
{
    ENV *env;

    env = new ENV();
    *EnvironmentHandlePointer = env;

    return SQL_SUCCESS;
}

SQLRETURN RDS_AllocDbc_Impl(
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

SQLRETURN RDS_AllocStmt_Impl(
    SQLHDBC        ConnectionHandle,
    SQLHSTMT *     StatementHandlePointer)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    STMT *stmt;
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV* env = (ENV*) dbc->env;

    RDS_SQLAllocHandle alloc_proc = (RDS_SQLAllocHandle) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLAllocHandle");

    stmt = new STMT();
    stmt->dbc = dbc;
    // Create underlying driver's statement handle
    (*alloc_proc)(SQL_HANDLE_STMT, dbc->wrapped_dbc, &stmt->wrapped_stmt);
    *StatementHandlePointer = stmt;

    dbc->stmt_list.emplace_back(stmt);

    return SQL_SUCCESS;
}

SQLRETURN RDS_AllocDesc_Impl(
    SQLHDBC        ConnectionHandle,
    SQLHANDLE *    DescriptorHandlePointer)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DESC *desc;
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV* env = (ENV*) dbc->env;

    RDS_SQLAllocHandle alloc_proc = (RDS_SQLAllocHandle) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLAllocHandle");

    desc = new DESC();
    desc->dbc = dbc;
    // Create underlying driver's descriptor handle
    (*alloc_proc)(SQL_HANDLE_STMT, dbc->wrapped_dbc, &desc->wrapped_desc);
    *DescriptorHandlePointer = desc;

    dbc->desc_list.emplace_back(desc);

    return SQL_SUCCESS;
}

SQLRETURN RDS_SQLEndTran_Impl(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    CompletionType)
{
    SQLRETURN ret = SQL_SUCCESS;
    ENV* env = nullptr;
    DBC* dbc = nullptr;
    RDS_SQLEndTran end_tran_proc = nullptr;
    switch (HandleType) {
        case SQL_HANDLE_DBC:
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            {
                dbc = (DBC*) Handle;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

                end_tran_proc = (RDS_SQLEndTran) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLEndTran");
                ret |= (*end_tran_proc)(HandleType, Handle, CompletionType);
            }
            break;
        case SQL_HANDLE_ENV:
            NULL_CHECK_HANDLE(Handle);
            {
                env = (ENV*) Handle;

                std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

                end_tran_proc = (RDS_SQLEndTran) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLEndTran");
                for (DBC* dbc : env->dbc_list) {
                        // TODO - May need enhanced error handling due to multiple DBCs
                        //   Should error out on the first?
                        ret |= (*end_tran_proc)(HandleType, Handle, CompletionType);
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

SQLRETURN RDS_FreeConnect_Impl(
    SQLHDBC        ConnectionHandle)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    bool has_free_stmt_errors = false, has_free_desc_errors = false;

    // Remove connection from environment
    env->dbc_list.remove(dbc); // TODO - Make this into a function within ENV to make use of locks

    if (env->wrapped_driver_handle) {
        // Clean up wrapped DBC
        RDS_SQLFreeHandle free_handle_proc = (RDS_SQLFreeHandle) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLFreeHandle");
        (*free_handle_proc)(SQL_HANDLE_DBC, dbc->wrapped_dbc);
    }

    // Clean up RDS related
    // TODO - e.g. clean up monitoring threads, etc

    delete dbc;
    return SQL_SUCCESS;
}

SQLRETURN RDS_FreeDesc_Impl(
    SQLHDESC       DescriptorHandle)
{
    NULL_CHECK_ENV_ACCESS_DESC(DescriptorHandle);
    DESC* desc = (DESC*) DescriptorHandle;
    DBC* dbc = desc->dbc;
    ENV* env = dbc->env;

    // Remove descriptor from connection
    dbc->desc_list.remove(desc);

    if (env->wrapped_driver_handle) {
        // Clean underlying Descriptors
        RDS_SQLFreeHandle free_handle_proc = (RDS_SQLFreeHandle) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLFreeHandle");
        (*free_handle_proc)(SQL_HANDLE_DESC, desc->wrapped_desc);
    }

    delete desc;
    return SQL_SUCCESS;
}

SQLRETURN RDS_FreeEnv_Impl(
    SQLHENV        EnvironmentHandle)
{
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV* env = (ENV*) EnvironmentHandle;

    // Clean tracked connections
    for (DBC* dbc : env->dbc_list) {
        RDS_FreeConnect_Impl(dbc);
    }

    if (env->wrapped_driver_handle) {
        // Clean underlying Env
        RDS_SQLFreeHandle free_handle_proc = (RDS_SQLFreeHandle) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLFreeHandle");
        (*free_handle_proc)(SQL_HANDLE_ENV, env->wrapped_env);
        // Clean up Dynamic Module
        RDS_FREE_MODULE(env->wrapped_driver_handle);
    }

    delete env;
    return SQL_SUCCESS;
}

SQLRETURN RDS_FreeStmt_Impl(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT* stmt = (STMT*) StatementHandle;
    DBC* dbc = stmt->dbc;
    ENV* env = dbc->env;

    // Remove statement from connection
    dbc->stmt_list.remove(stmt);

    if (env->wrapped_driver_handle) {
        // Clean underlying Statements
        RDS_SQLFreeHandle free_handle_proc = (RDS_SQLFreeHandle) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLFreeHandle");
        (*free_handle_proc)(SQL_HANDLE_STMT, stmt->wrapped_stmt);
    }

    delete stmt;
    return SQL_SUCCESS;
}

SQLRETURN RDS_GetConnectAttr_Impl(
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
    if (dbc->wrapped_dbc && env->wrapped_driver_handle) {
        RDS_SQLGetConnectAttr get_connect_attr_proc = (RDS_SQLGetConnectAttr) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetConnectAttr");
        ret = (*get_connect_attr_proc)(dbc->wrapped_dbc, Attribute, ValuePtr, BufferLength, StringLengthPtr);
    }
    // Otherwise get from the DBC's attribute map
    else if (dbc->attr_map.find(Attribute) != dbc->attr_map.end()) {
        std::pair<SQLPOINTER, SQLINTEGER> value_pair = dbc->attr_map.at(Attribute);
        if (value_pair.second == sizeof(SQLSMALLINT)) {
            *((SQLUSMALLINT *) ValuePtr) = static_cast<SQLSMALLINT>(reinterpret_cast<intptr_t>(value_pair.first));
        } else if (value_pair.second == sizeof(SQLUINTEGER) || value_pair.second == 0) {
            *((SQLUINTEGER *) ValuePtr) = static_cast<SQLUINTEGER>(reinterpret_cast<uintptr_t>(value_pair.first));
        } else {
            snprintf((char *) ValuePtr, (size_t) BufferLength, "%s", (char *) value_pair.first);
            if (value_pair.second >= BufferLength) {
                    ret = SQL_SUCCESS_WITH_INFO;
            }
        }
        ret = SQL_SUCCESS;
    }
    return ret;
}

SQLRETURN RDS_SQLSetConnectAttr_Impl(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    // Track new value
    dbc->attr_map.insert_or_assign(Attribute, std::make_pair(ValuePtr, StringLength));

    // If already connected, apply value to underlying DBC, otherwise will apply on connect
    if (dbc->wrapped_dbc && env->wrapped_driver_handle) {
        RDS_SQLSetConnectAttr set_connect_attr_proc = (RDS_SQLSetConnectAttr) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetConnectAttr");
        SQLRETURN ret = (*set_connect_attr_proc)(dbc->wrapped_dbc, Attribute, ValuePtr, StringLength);
        return ret;
    }

    return SQL_SUCCESS;
}

// Windows GUI Related
// TODO - Impl ConfigDriver
// Later process
BOOL ConfigDriver(SQLHWND hwndParent, WORD fRequest, LPCSTR lpszDriver, LPCSTR lpszArgs, LPSTR lpszMsg,
                WORD cbMsgMax, WORD* pcbMsgOut) {
    NOT_IMPLEMENTED;
}

// TODO - Impl ConfigDSN
// Later process
BOOL ConfigDSN(SQLHWND hwndParent, WORD fRequest, LPCSTR lpszDriver, LPCSTR lpszAttributes) {
    NOT_IMPLEMENTED;
}

// ODBC Functions
SQLRETURN SQL_API SQLAllocConnect(
    SQLHENV        EnvironmentHandle,
    SQLHDBC *      ConnectionHandle)
{
    return RDS_AllocDbc_Impl(EnvironmentHandle, ConnectionHandle);
};

SQLRETURN SQL_API SQLAllocEnv(
    SQLHENV *      EnvironmentHandle)
{
    return RDS_AllocEnv_Impl(EnvironmentHandle);
}

SQLRETURN SQL_API SQLAllocHandle(
    SQLSMALLINT    HandleType,
    SQLHANDLE      InputHandle,
    SQLHANDLE *    OutputHandlePtr)
{
    SQLRETURN ret = SQL_ERROR;
    switch (HandleType) {
        case SQL_HANDLE_ENV:
            ret = RDS_AllocEnv_Impl(OutputHandlePtr);
            break;
        case SQL_HANDLE_DBC:
            ret = RDS_AllocDbc_Impl(InputHandle, OutputHandlePtr);
            break;
        case SQL_HANDLE_STMT:
            ret = RDS_AllocStmt_Impl(InputHandle, OutputHandlePtr);
            break;
        case SQL_HANDLE_DESC:
            ret = RDS_AllocDesc_Impl(InputHandle, OutputHandlePtr);
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
    return RDS_AllocStmt_Impl(ConnectionHandle, StatementHandle);
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

    RDS_SQLBindCol bind_col_proc = (RDS_SQLBindCol) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLBindCol");
    SQLRETURN ret = (*bind_col_proc)(stmt->wrapped_stmt, ColumnNumber, TargetType, TargetValuePtr, BufferLength, StrLen_or_IndPtr);
    return ret;
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

    RDS_SQLBindParameter bind_para_proc = (RDS_SQLBindParameter) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLBindParameter");
    SQLRETURN ret = (*bind_para_proc)(stmt->wrapped_stmt, ParameterNumber, InputOutputType, ValueType, ParameterType, ColumnSize, DecimalDigits, ParameterValuePtr, BufferLength, StrLen_or_IndPtr);
    return ret;
}

// TODO Maybe - Impl SQLBrowseConnect
// Both PostgreSQL AND MySQL do NOT implement this
SQLRETURN SQL_API SQLBrowseConnect(
    SQLHDBC        ConnectionHandle,
    SQLCHAR *      InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLCHAR *      OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr)
{
    NOT_IMPLEMENTED;
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

    RDS_SQLBulkOperations bulk_operations_proc = (RDS_SQLBulkOperations) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLBulkOperations");
    SQLRETURN ret = (*bulk_operations_proc)(stmt->wrapped_stmt, Operation);
    return ret;
}

SQLRETURN SQL_API SQLCancel(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLCancel cancel_proc = (RDS_SQLCancel) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLCancel");
    SQLRETURN ret = (*cancel_proc)(stmt->wrapped_stmt);
    return ret;
}

// TODO Maybe - Impl SQLCancelHandle
// PostgreSQL does NOT implement this
// MySQL only supports SQL_HANDLE_STMT, which calls [SQLCancel]
SQLRETURN SQL_API SQLCancelHandle(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle)
{
    STMT *stmt;
    DBC *dbc;
    ENV *env;
    SQLRETURN ret = SQL_ERROR;
    RDS_SQLCancel cancel_proc = nullptr;
    switch (HandleType) {
        case SQL_HANDLE_STMT:
            NULL_CHECK_ENV_ACCESS_STMT(Handle);
            {
                stmt = (STMT*) Handle;
                dbc = (DBC*) stmt->dbc;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

                cancel_proc = (RDS_SQLCancel) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLCancel");
                ret = (*cancel_proc)(stmt->wrapped_stmt);
            }
            break;
        case SQL_HANDLE_ENV:
        case SQL_HANDLE_DBC:
        case SQL_HANDLE_DESC:
        default:
            // TODO - Set Error
            ret = SQL_ERROR;
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

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLCloseCursor close_cursor_proc = (RDS_SQLCloseCursor) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLCloseCursor");
    SQLRETURN ret = (*close_cursor_proc)(stmt->wrapped_stmt);
    return ret;
}

SQLRETURN SQL_API SQLColAttribute(
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

    RDS_SQLColAttribute col_attribute_proc = (RDS_SQLColAttribute) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLColAttribute");
    SQLRETURN ret = (*col_attribute_proc)(stmt->wrapped_stmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr);
    return ret;
}

SQLRETURN SQL_API SQLColAttributes(
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

    RDS_SQLColAttributes col_attributes_proc = (RDS_SQLColAttributes) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLColAttributes");
    SQLRETURN ret = (*col_attributes_proc)(stmt->wrapped_stmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr);
    return ret;
}

SQLRETURN SQL_API SQLColumnPrivileges(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLCHAR *      ColumnName,
    SQLSMALLINT    NameLength4)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLColumnPrivileges column_privileges_proc = (RDS_SQLColumnPrivileges) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLColumnPrivileges");
    SQLRETURN ret = (*column_privileges_proc)(stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, ColumnName, NameLength4);
    return ret;
}

SQLRETURN SQL_API SQLColumns(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLCHAR *      ColumnName,
    SQLSMALLINT    NameLength4)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLColumns columns_proc = (RDS_SQLColumns) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLColumns");
    SQLRETURN ret = (*columns_proc)(stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, ColumnName, NameLength4);
    return ret;
}

// TODO Maybe - Impl SQLCompleteAsync
// Both PostgreSQL AND MySQL do NOT implement this
SQLRETURN SQL_API SQLCompleteAsync(
    SQLSMALLINT   HandleType,
    SQLHANDLE     Handle,
    RETCODE *     AsyncRetCodePtr)
{
    NOT_IMPLEMENTED;
}

// TODO - Impl
// NOTE - ServerName refers to DSN name
SQLRETURN SQL_API SQLConnect(
    SQLHDBC        ConnectionHandle,
    SQLCHAR *      ServerName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      UserName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      Authentication,
    SQLSMALLINT    NameLength3)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC* dbc = (DBC*) ConnectionHandle;
    ENV* env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    SQLRETURN ret = SQL_ERROR;
    // Error if handle is already connected
    if (CONN_NOT_CONNECTED != dbc->conn_status) {
        // TODO - Error info
        return SQL_ERROR;
    }

    // TODO - Read DSN & Load Information

    return SQL_ERROR;
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

    RDS_SQLCopyDesc copy_desc_proc = (RDS_SQLCopyDesc) RDS_GET_FUNC(src_env->wrapped_driver_handle, "SQLCopyDesc");
    SQLRETURN ret = (*copy_desc_proc)(src_desc->wrapped_desc, dst_desc->wrapped_desc);

    // Move to use new DBC
    if (dst_desc->dbc) {
        dst_desc->dbc->desc_list.remove(dst_desc);
    }
    src_dbc->desc_list.emplace_back(dst_desc);
    dst_desc->dbc = src_desc->dbc;

    return ret;
}

// TODO Maybe - Impl SQLDataSources
// Both PostgreSQL AND MySQL do NOT implement this
SQLRETURN SQL_API SQLDataSources(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLCHAR *      ServerName,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  NameLength1Ptr,
    SQLCHAR *      Description,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  NameLength2Ptr)
{
    NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLDescribeCol(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLCHAR *      ColumnName,
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

    RDS_SQLDescribeCol describe_col_proc = (RDS_SQLDescribeCol) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLDescribeCol");
    SQLRETURN ret = (*describe_col_proc)(stmt->wrapped_stmt, ColumnNumber, ColumnName, BufferLength, NameLengthPtr, DataTypePtr, ColumnSizePtr, DecimalDigitsPtr, NullablePtr);
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

    RDS_SQLDescribeParam describe_param_proc = (RDS_SQLDescribeParam) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLDescribeParam");
    SQLRETURN ret = (*describe_param_proc)(stmt->wrapped_stmt, ParameterNumber, DataTypePtr, ParameterSizePtr, DecimalDigitsPtr, NullablePtr);
    return ret;
}

SQLRETURN SQL_API SQLDisconnect(
    SQLHDBC        ConnectionHandle)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    RDS_SQLDisconnect disconnect_proc = (RDS_SQLDisconnect) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLDisconnect");
    SQLRETURN ret = (*disconnect_proc)(dbc->wrapped_dbc);
    return ret;
}

/*
    TODO - Not fully implemented
    - Needs Connection String Parsing, underlying connection is hardcoded right now
*/
SQLRETURN SQL_API SQLDriverConnect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLCHAR *      InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLCHAR *      OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr,
    SQLUSMALLINT   DriverCompletion)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    SQLRETURN ret = SQL_ERROR;
    bool has_env_attr_errors = false;

    // Connection is already established
    if (CONN_NOT_CONNECTED != dbc->conn_status) {
        // TODO - Error info
        return SQL_ERROR;
    }

    // TODO - Read DSN & Load Information

    // TODO - Will need to modify this function to only put if absent
    //   Ideally we would still want this at the lowest level, as the base plugin
    //   But since we will have other plugins such as IAM replacing the PWD,
    //   we don't want the DSN to replace that generated token
    //
    //   For now, we will simply remove the Driver & DSN from the connection string
    dbc->conn_attr = ConnectionStringHelper::ParseConnectionString(AS_RDS_STR(InConnectionString));
    dbc->conn_attr.erase(KEY_DRIVER);  // Temporary solution, see above
    dbc->conn_attr.erase(KEY_DSN);     // Temporary solution, see above
    if (dbc->conn_attr.find(KEY_BASE_DRIVER) != dbc->conn_attr.end()) {
        // TODO - Need to ensure the paths (slashes) are correct per OS
        RDS_STR driver_name = dbc->conn_attr.at(KEY_BASE_DRIVER);

        // Load Module to Env if empty
        // TODO - Put into function, need to lock Env
        if (env->wrapped_driver_path.empty()) {
            env->wrapped_driver_path = driver_name;
            env->wrapped_driver_handle = RDS_LOAD_MODULE_DEFAULTS(driver_name.c_str());
        } else if (driver_name != env->wrapped_driver_path) {
            // TODO - Set Error, can only use 1 underlying driver per Environment
            return SQL_ERROR;
        }
    } else {
        // TODO - No underlying driver in ConnStr
        //   set error and return?
        // OR
        //   check if ENV has an underlying driver already
        NOT_IMPLEMENTED;
    }

    // TODO - Needs to be taken out and stored into cached function map in ENV
    RDS_SQLAllocHandle alloc_proc = (RDS_SQLAllocHandle) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLAllocHandle");
    RDS_SQLSetEnvAttr set_env_proc = (RDS_SQLSetEnvAttr) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetEnvAttr");

    // Initialize Wrapped ENV
    // Create Wrapped HENV for Wrapped HDBC if not already allocated
    if (!env->wrapped_env) {
        (*alloc_proc)(SQL_HANDLE_ENV, nullptr, &env->wrapped_env);
        // Apply Tracked Environment Attributes
        for (auto const& [key, val] : env->attr_map) {
            has_env_attr_errors |= (*set_env_proc)(env->wrapped_env, key, val.first, val.second);
        }
    }

    // Initialize Plugins
    if (!dbc->plugin_head) {
        BasePlugin* plugin_head = new BasePlugin(dbc);
        BasePlugin* next_plugin;

        // TODO - Grabbing which plugins to initialize will come from a KEY=<Plugin_A, ..., Plugin_Z>;

        // Auth Plugins
        if (dbc->conn_attr.find(KEY_AUTH_TYPE) != dbc->conn_attr.end()) {
            AuthType type = AuthTypeFromString(dbc->conn_attr.at(KEY_AUTH_TYPE));
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

    ret = dbc->plugin_head->Connect(WindowHandle, OutConnectionString, BufferLength, StringLength2Ptr, DriverCompletion);
    return ret;
}

// TODO Maybe - Impl SQLDrivers
// Not implemented in MySQL or PostgreSQL ODBC
SQLRETURN SQL_API SQLDrivers(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLCHAR *      DriverDescription,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  DescriptionLengthPtr,
    SQLCHAR *      DriverAttributes,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  AttributesLengthPtr)
{
    NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLEndTran(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    CompletionType)
{
    return RDS_SQLEndTran_Impl(HandleType, Handle, CompletionType);
}

// TODO Maybe - Impl SQLError
// Not implemented in MySQL or PostgreSQL ODBC
// Deprecated, 2.x should map to SQLDiagRec
SQLRETURN SQL_API SQLError(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLHSTMT       StatementHandle,
    SQLCHAR *      SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLCHAR *      MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr)
{
    NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLExecDirect(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      StatementText,
    SQLINTEGER     TextLength)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLExecDirect exec_proc = (RDS_SQLExecDirect) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLExecDirect");
    SQLRETURN ret = (*exec_proc)(stmt->wrapped_stmt, StatementText, TextLength);
    return ret;
}

SQLRETURN SQL_API SQLExecute(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLExecute exec_proc = (RDS_SQLExecute) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLExecute");
    SQLRETURN ret = (*exec_proc)(stmt->wrapped_stmt);
    return ret;
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

    RDS_SQLExecute exec_proc = (RDS_SQLExecute) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLExecute");
    SQLRETURN ret = (*exec_proc)(stmt->wrapped_stmt);
    return ret;
}

SQLRETURN SQL_API SQLFetch(
    SQLHSTMT        StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLFetch fetch_proc = (RDS_SQLFetch) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLFetch");
    SQLRETURN ret = (*fetch_proc)(stmt->wrapped_stmt);
    return ret;
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

    RDS_SQLFetchScroll fetch_scroll_proc = (RDS_SQLFetchScroll) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLFetchScroll");
    SQLRETURN ret = (*fetch_scroll_proc)(stmt->wrapped_stmt, FetchOrientation, FetchOffset);
    return ret;
}

SQLRETURN SQL_API SQLForeignKeys(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      PKCatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      PKSchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      PKTableName,
    SQLSMALLINT    NameLength3,
    SQLCHAR *      FKCatalogName,
    SQLSMALLINT    NameLength4,
    SQLCHAR *      FKSchemaName,
    SQLSMALLINT    NameLength5,
    SQLCHAR *      FKTableName,
    SQLSMALLINT    NameLength6)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLForeignKeys foreign_keys_proc = (RDS_SQLForeignKeys) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLForeignKeys");
    SQLRETURN ret = (*foreign_keys_proc)(stmt->wrapped_stmt, PKCatalogName, NameLength1, PKSchemaName, NameLength2, PKTableName, NameLength3, FKCatalogName, NameLength4, FKSchemaName, NameLength5, FKTableName, NameLength6);
    return ret;
}

SQLRETURN SQL_API SQLFreeConnect(
    SQLHDBC        ConnectionHandle)
{
    return RDS_FreeConnect_Impl(ConnectionHandle);
}

SQLRETURN SQL_API SQLFreeEnv(
    SQLHENV        EnvironmentHandle)
{
    return RDS_FreeEnv_Impl(EnvironmentHandle);
}

SQLRETURN SQL_API SQLFreeHandle(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle)
{
    SQLRETURN ret = SQL_ERROR;
    switch (HandleType) {
        case SQL_HANDLE_DBC:
            ret = RDS_FreeConnect_Impl(Handle);
            break;
        case SQL_HANDLE_DESC:
            ret = RDS_FreeDesc_Impl(Handle);
            break;
        case SQL_HANDLE_ENV:
            ret = RDS_FreeEnv_Impl(Handle);
            break;
        case SQL_HANDLE_STMT:
            ret = RDS_FreeStmt_Impl(Handle);
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
    return RDS_FreeStmt_Impl(StatementHandle);
}

SQLRETURN SQL_API SQLGetConnectAttr(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    return RDS_GetConnectAttr_Impl(ConnectionHandle, Attribute, ValuePtr, BufferLength, StringLengthPtr);
}

SQLRETURN SQL_API SQLGetConnectOption(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr)
{
    return RDS_GetConnectAttr_Impl(
        ConnectionHandle,
        Attribute,
        ValuePtr,
        ((Attribute == SQL_ATTR_CURRENT_CATALOG) ? SQL_MAX_OPTION_STRING_LENGTH : 0),
        NULL
    );
}

SQLRETURN SQL_API SQLGetCursorName(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CursorName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLGetCursorName get_cursor_name_proc = (RDS_SQLGetCursorName) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetCursorName");
    SQLRETURN ret = (*get_cursor_name_proc)(stmt->wrapped_stmt, CursorName, BufferLength, NameLengthPtr);
    return ret;
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

    RDS_SQLGetData get_data_proc = (RDS_SQLGetData) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetData");
    SQLRETURN ret = (*get_data_proc)(stmt->wrapped_stmt, Col_or_Param_Num, TargetType, TargetValuePtr, BufferLength, StrLen_or_IndPtr);
    return ret;
}

SQLRETURN SQL_API SQLGetDescField(
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

    RDS_SQLGetDescField get_desc_field_proc = (RDS_SQLGetDescField) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDescField");
    SQLRETURN ret = (*get_desc_field_proc)(desc->wrapped_desc, RecNumber, FieldIdentifier, ValuePtr, BufferLength, StringLengthPtr);
    return ret;
}

SQLRETURN SQL_API SQLGetDescRec(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLCHAR *      Name,
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

    RDS_SQLGetDescRec get_desc_rec_proc = (RDS_SQLGetDescRec) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDescRec");
    SQLRETURN ret = (*get_desc_rec_proc)(desc->wrapped_desc, RecNumber, Name, BufferLength, StringLengthPtr, TypePtr, SubTypePtr, LengthPtr, PrecisionPtr, ScalePtr, NullablePtr);
    return ret;
}

SQLRETURN SQL_API SQLGetDiagField(
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
    RDS_SQLGetDiagField get_diag_field_proc = nullptr;
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
                if (env->wrapped_driver_handle && env->wrapped_env) {
                        has_underlying_driver_alloc = true;
                        get_diag_field_proc = (RDS_SQLGetDiagField) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDiagField");
                        ret = (*get_diag_field_proc)(HandleType, env->wrapped_env, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr);
                }
            }
            break;
        case SQL_HANDLE_DBC:
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            {
                dbc = (DBC*) Handle;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

                if (env->wrapped_driver_handle && dbc->wrapped_dbc) {
                        has_underlying_driver_alloc = true;
                        get_diag_field_proc = (RDS_SQLGetDiagField) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDiagField");
                        ret = (*get_diag_field_proc)(HandleType, dbc->wrapped_dbc, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr);
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

                if (env->wrapped_driver_handle && stmt->wrapped_stmt) {
                        has_underlying_driver_alloc = true;
                        get_diag_field_proc = (RDS_SQLGetDiagField) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDiagField");
                        ret = (*get_diag_field_proc)(HandleType, stmt->wrapped_stmt, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr);
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

                if (env->wrapped_driver_handle && desc->wrapped_desc) {
                        has_underlying_driver_alloc = true;
                        get_diag_field_proc = (RDS_SQLGetDiagField) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDiagField");
                        ret = (*get_diag_field_proc)(HandleType, desc->wrapped_desc, RecNumber, DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr);
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

SQLRETURN SQL_API SQLGetDiagRec(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLCHAR *      SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLCHAR *      MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr)
{
    DESC *desc = nullptr;
    STMT *stmt = nullptr;
    DBC *dbc = nullptr;
    ENV *env = nullptr;
    RDS_SQLGetDiagRec get_diag_rec_proc = nullptr;
    SQLRETURN ret = SQL_ERROR;
    SQLULEN len = 0, value = 0;
    const char *char_value = nullptr;
    ERR_INFO* err = nullptr;
    bool has_underlying_driver_alloc = false;

    switch (HandleType) {
        case SQL_HANDLE_ENV:
            NULL_CHECK_HANDLE(Handle);
            {
                env = (ENV*) Handle;

                std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

                if (env->wrapped_driver_handle && env->wrapped_env) {
                        has_underlying_driver_alloc = true;
                        get_diag_rec_proc = (RDS_SQLGetDiagRec) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDiagRec");
                        ret = (*get_diag_rec_proc)(HandleType, env->wrapped_env, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr);
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

                if (env->wrapped_driver_handle && dbc->wrapped_dbc) {
                        has_underlying_driver_alloc = true;
                        get_diag_rec_proc = (RDS_SQLGetDiagRec) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDiagRec");
                        ret = (*get_diag_rec_proc)(HandleType, dbc->wrapped_dbc, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr);
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

                if (env->wrapped_driver_handle && stmt->wrapped_stmt) {
                        has_underlying_driver_alloc = true;
                        get_diag_rec_proc = (RDS_SQLGetDiagRec) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDiagRec");
                        ret = (*get_diag_rec_proc)(HandleType, stmt->wrapped_stmt, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr);
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

                if (env->wrapped_driver_handle && desc->wrapped_desc) {
                        has_underlying_driver_alloc = true;
                        get_diag_rec_proc = (RDS_SQLGetDiagRec) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetDiagRec");
                        ret = (*get_diag_rec_proc)(HandleType, desc->wrapped_desc, RecNumber, SQLState, NativeErrorPtr, MessageText, BufferLength, TextLengthPtr);
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
            snprintf((char *) SQLState, MAX_SQL_STATE_LEN, "%s", err->sqlstate);
        }
        if (MessageText) {
            SQLLEN err_len = strlen(err->error_msg);
            snprintf((char *) MessageText, (size_t) BufferLength, "%s", (char *) err->error_msg);
            if (err_len >= BufferLength) {
                    ret = SQL_SUCCESS_WITH_INFO;
            }
            if (TextLengthPtr) {
                    *((SQLSMALLINT *) TextLengthPtr) = (SQLSMALLINT) err_len;
            }
        }
        if (NativeErrorPtr) {
            *((SQLUINTEGER *) NativeErrorPtr) = (SQLUINTEGER) err->error_num;
        }
    } else if (!has_underlying_driver_alloc) {
        // No Data
        if (SQLState) {
            snprintf((char *) SQLState, MAX_SQL_STATE_LEN, "%s", NO_DATA_SQL_STATE);
        }
        if (MessageText) {
            snprintf((char *) MessageText, 0, "%s", "");
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

    if (env->attr_map.find(Attribute) != env->attr_map.end()) {
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

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    // Query underlying driver if connection is established
    if (dbc && dbc->wrapped_dbc) {
        ENV* env = (ENV*) dbc->env;
        RDS_SQLGetFunctions get_func_proc = (RDS_SQLGetFunctions) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetFunctions");
        SQLRETURN ret = (*get_func_proc)(dbc->wrapped_dbc, FunctionId, SupportedPtr);
        return ret;
    }

    // TODO - THIS IS HARDCODED
    // Will need to keep track of a map of the current implemented functions
    // as ODBC adds/removes functions in the future
    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetInfo(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   InfoType,
    SQLPOINTER     InfoValuePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr)
{
    SQLRETURN ret = SQL_ERROR;
    SQLULEN len = 0, value = 0;
    const char *char_value = nullptr;
    char odbcver[16];

    // Query underlying driver if connection is established
    DBC* dbc = (DBC*) ConnectionHandle;

    {
        std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
        if (dbc && dbc->wrapped_dbc) {
            ENV* env = (ENV*) dbc->env;
            RDS_SQLGetInfo get_info_proc = (RDS_SQLGetInfo) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetInfo");
            ret = (*get_info_proc)(dbc->wrapped_dbc, InfoType, InfoValuePtr, BufferLength, StringLengthPtr);
            return ret;
        }
    }

    // Get info for shell driver
    switch (InfoType) {
        case SQL_DRIVER_ODBC_VER:
            snprintf(odbcver, 16, "%02x.%02x", ODBCVER / 256, ODBCVER % 256);
            char_value = odbcver;
            break;
        case SQL_DM_VER:
            // TODO - Not a real implementation
            len = 0;
            break;
        // TODO - Add other cases as needed
        default:
            NOT_IMPLEMENTED;
    }
    ret = SQL_SUCCESS;

    // Pass info back to caller
    if (char_value) {
        len = strlen(char_value);
        if (InfoValuePtr) {
            snprintf((char *) InfoValuePtr, (size_t) BufferLength, "%s", char_value);
            if (len >= BufferLength) {
                    ret = SQL_SUCCESS_WITH_INFO;
            }
        }
        if (StringLengthPtr) {
            *StringLengthPtr = (SQLSMALLINT) len;
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

SQLRETURN SQL_API SQLGetStmtAttr(
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

    RDS_SQLGetStmtAttr get_stmt_attr_proc = (RDS_SQLGetStmtAttr) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetStmtAttr");
    SQLRETURN ret = (*get_stmt_attr_proc)(stmt->wrapped_stmt, Attribute, ValuePtr, BufferLength, StringLengthPtr);
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

    RDS_SQLGetStmtOption get_stmt_option_proc = (RDS_SQLGetStmtOption) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetStmtOption");
    SQLRETURN ret = (*get_stmt_option_proc)(stmt->wrapped_stmt, Attribute, ValuePtr);
    return ret;
}

SQLRETURN SQL_API SQLGetTypeInfo(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    DataType)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLGetTypeInfo get_type_info_proc = (RDS_SQLGetTypeInfo) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLGetTypeInfo");
    SQLRETURN ret = (*get_type_info_proc)(stmt->wrapped_stmt, DataType);
    return ret;
}

SQLRETURN SQL_API SQLMoreResults(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLMoreResults more_results_proc = (RDS_SQLMoreResults) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLMoreResults");
    SQLRETURN ret = (*more_results_proc)(stmt->wrapped_stmt);
    return ret;
}

SQLRETURN SQL_API SQLNativeSql(
    SQLHDBC        ConnectionHandle,
    SQLCHAR *      InStatementText,
    SQLINTEGER     TextLength1,
    SQLCHAR *      OutStatementText,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   TextLength2Ptr)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    RDS_SQLNativeSql native_sql_proc = (RDS_SQLNativeSql) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLNativeSql");
    SQLRETURN ret = (*native_sql_proc)(dbc->wrapped_dbc, InStatementText, TextLength1, OutStatementText, BufferLength, TextLength2Ptr);
    return ret;
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

    RDS_SQLNumParams num_params_proc = (RDS_SQLNumParams) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLNumParams");
    SQLRETURN ret = (*num_params_proc)(stmt->wrapped_stmt, ParameterCountPtr);
    return ret;
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

    RDS_SQLNumResultCols num_results_col_proc = (RDS_SQLNumResultCols) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLNumResultCols");
    SQLRETURN ret = (*num_results_col_proc)(stmt->wrapped_stmt, ColumnCountPtr);
    return ret;
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

    RDS_SQLParamData param_data_proc = (RDS_SQLParamData) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLParamData");
    SQLRETURN ret = (*param_data_proc)(stmt->wrapped_stmt, ValuePtrPtr);
    return ret;
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

    RDS_SQLParamOptions param_options_proc = (RDS_SQLParamOptions) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLParamOptions");
    SQLRETURN ret = (*param_options_proc)(stmt->wrapped_stmt, Crow, FetchOffsetPtr);
    return ret;
}

SQLRETURN SQL_API SQLPrepare(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      StatementText,
    SQLINTEGER     TextLength)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLPrepare prep_proc = (RDS_SQLPrepare) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLPrepare");
    SQLRETURN ret = (*prep_proc)(stmt->wrapped_stmt, StatementText, TextLength);
    return ret;
}

SQLRETURN SQL_API SQLPrimaryKeys(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLPrimaryKeys primary_keys_proc = (RDS_SQLPrimaryKeys) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLPrimaryKeys");
    SQLRETURN ret = (*primary_keys_proc)(stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3);
    return ret;
}

SQLRETURN SQL_API SQLProcedureColumns(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      ProcName,
    SQLSMALLINT    NameLength3,
    SQLCHAR *      ColumnName,
    SQLSMALLINT    NameLength4)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLProcedureColumns procedure_columns_proc = (RDS_SQLProcedureColumns) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLProcedureColumns");
    SQLRETURN ret = (*procedure_columns_proc)(stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, ProcName, NameLength3, ColumnName, NameLength4);
    return ret;
}

SQLRETURN SQL_API SQLProcedures(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      ProcName,
    SQLSMALLINT    NameLength3)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLProcedures procedures_proc = (RDS_SQLProcedures) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLProcedures");
    SQLRETURN ret = (*procedures_proc)(stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, ProcName, NameLength3);
    return ret;
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

    RDS_SQLPutData put_data_proc = (RDS_SQLPutData) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLPutData");
    SQLRETURN ret = (*put_data_proc)(stmt->wrapped_stmt, DataPtr, StrLen_or_Ind);
    return ret;
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

    RDS_SQLRowCount row_count_proc = (RDS_SQLRowCount) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLRowCount");
    SQLRETURN ret = (*row_count_proc)(stmt->wrapped_stmt, RowCountPtr);
    return ret;
}

SQLRETURN SQL_API SQLSetConnectAttr(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    return RDS_SQLSetConnectAttr_Impl(ConnectionHandle, Attribute, ValuePtr, StringLength);
}

SQLRETURN SQL_API SQLSetConnectOption(
    SQLHDBC        ConnectionHandle,
    SQLSMALLINT    Option,
    SQLPOINTER     Param)
{
    SQLINTEGER value_len= 0;
    if (Option == SQL_ATTR_CURRENT_CATALOG) {
        value_len= SQL_NTS;
    }
    return RDS_SQLSetConnectAttr_Impl(ConnectionHandle, Option, Param, value_len);
}

SQLRETURN SQL_API SQLSetCursorName(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CursorName,
    SQLSMALLINT    NameLength)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLSetCursorName set_cursor_name_proc = (RDS_SQLSetCursorName) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetCursorName");
    SQLRETURN ret = (*set_cursor_name_proc)(stmt->wrapped_stmt, CursorName, NameLength);
    return ret;
}

SQLRETURN SQL_API SQLSetDescField(
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

    RDS_SQLSetDescField set_desc_field_proc = (RDS_SQLSetDescField) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetDescField");
    SQLRETURN ret = (*set_desc_field_proc)(desc->wrapped_desc, RecNumber, FieldIdentifier, ValuePtr, BufferLength);
    return ret;
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

    RDS_SQLSetDescRec set_desc_rec_proc = (RDS_SQLSetDescRec) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetDescRec");
    SQLRETURN ret = (*set_desc_rec_proc)(desc->wrapped_desc, RecNumber, Type, SubType, Length, Precision, Scale, DataPtr, StringLengthPtr, IndicatorPtr);
    return ret;
}

SQLRETURN SQL_API SQLSetEnvAttr(
    SQLHENV        EnvironmentHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    NULL_CHECK_HANDLE(EnvironmentHandle);
    ENV *env = (ENV*) EnvironmentHandle;

    std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

    // Track new value
    env->attr_map.insert_or_assign(Attribute, std::make_pair(ValuePtr, StringLength));

    // Update existing connections environments
    if (env->wrapped_driver_handle) {
        RDS_SQLSetEnvAttr set_env_proc = (RDS_SQLSetEnvAttr) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetEnvAttr");
        (*set_env_proc)(env->wrapped_env, Attribute, ValuePtr, StringLength);
    }

    return SQL_SUCCESS;
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

    RDS_SQLSetParam set_param_proc = (RDS_SQLSetParam) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetParam");
    SQLRETURN ret = (*set_param_proc)(stmt->wrapped_stmt, ParameterNumber, ValueType, ParameterType, ColumnSize, DecimalDigits, ParameterValuePtr, StrLen_or_IndPtr);
    return ret;
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

    RDS_SQLSetPos set_pos_proc = (RDS_SQLSetPos) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetPos");
    SQLRETURN ret = (*set_pos_proc)(stmt->wrapped_stmt, RowNumber, Operation, LockType);
    return ret;
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

    RDS_SQLSetScrollOptions set_scroll_options_proc = (RDS_SQLSetScrollOptions) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetScrollOptions");
    SQLRETURN ret = (*set_scroll_options_proc)(stmt->wrapped_stmt, Concurrency, KeysetSize, RowsetSize);
    return ret;
}

SQLRETURN SQL_API SQLSetStmtAttr(
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

    RDS_SQLSetStmtAttr set_scroll_options_proc = (RDS_SQLSetStmtAttr) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetStmtAttr");
    SQLRETURN ret = (*set_scroll_options_proc)(stmt->wrapped_stmt, Attribute, ValuePtr, StringLength);
    return ret;
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

    RDS_SQLSetStmtOption set_stmt_option_proc = (RDS_SQLSetStmtOption) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetStmtOption");
    SQLRETURN ret = (*set_stmt_option_proc)(stmt->wrapped_stmt, Option, Param);
    return ret;
}

SQLRETURN SQL_API SQLSpecialColumns(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    IdentifierType,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLSMALLINT    Scope,
    SQLSMALLINT    Nullable)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLSpecialColumns special_columns_proc = (RDS_SQLSpecialColumns) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSpecialColumns");
    SQLRETURN ret = (*special_columns_proc)(stmt->wrapped_stmt, IdentifierType, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, Scope, Nullable);
    return ret;
}

SQLRETURN SQL_API SQLStatistics(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLUSMALLINT   Unique,
    SQLUSMALLINT   Reserved)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLStatistics statistics_proc = (RDS_SQLStatistics) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLStatistics");
    SQLRETURN ret = (*statistics_proc)(stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, Unique, Reserved);
    return ret;
}

SQLRETURN SQL_API SQLTablePrivileges(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLTablePrivileges table_privileges_proc = (RDS_SQLTablePrivileges) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLTablePrivileges");
    SQLRETURN ret = (*table_privileges_proc)(stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3);
    return ret;
}

SQLRETURN SQL_API SQLTables(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLCHAR *      TableType,
    SQLSMALLINT    NameLength4)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RDS_SQLTables tables_proc = (RDS_SQLTables) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLTables");
    SQLRETURN ret = (*tables_proc)(stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, TableType, NameLength4);
    return ret;
}

SQLRETURN SQL_API SQLTransact(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   CompletionType)
{
    if (nullptr == EnvironmentHandle && nullptr == ConnectionHandle) {
        return SQL_INVALID_HANDLE;
    }

    return RDS_SQLEndTran_Impl(
        ConnectionHandle ? SQL_HANDLE_DBC : SQL_HANDLE_ENV,
        ConnectionHandle ? ConnectionHandle : EnvironmentHandle,
        CompletionType
    );
}
