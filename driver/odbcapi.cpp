#include "util/auth_provider.h"

#include "odbcapi.h"

#include "plugin/iam/iam_auth_plugin.h"

#include "util/connection_string_helper.h"
#include "util/connection_string_keys.h"
#include "util/odbc_dsn_helper.h"
#include "util/rds_lib_loader.h"
#include "util/rds_strings.h"

#include "driver.h"

#include <cstring>

#include <iostream>

// RDS Functions
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

    stmt = new STMT();
    stmt->dbc = dbc;
    // Create underlying driver's statement handle
    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLAllocHandle>(AS_RDS_STR("SQLAllocHandle"),
        SQL_HANDLE_STMT, dbc->wrapped_dbc, &stmt->wrapped_stmt
    );
    RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    desc = new DESC();
    desc->dbc = dbc;
    // Create underlying driver's descriptor handle
    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLAllocHandle>(AS_RDS_STR("SQLAllocHandle"),
        SQL_HANDLE_DESC, dbc->wrapped_dbc, &desc->wrapped_desc
    );
    RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
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
    RdsLibResult res;
    switch (HandleType) {
        case SQL_HANDLE_DBC:
            NULL_CHECK_ENV_ACCESS_DBC(Handle);
            {
                dbc = (DBC*) Handle;
                env = (ENV*) dbc->env;

                std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

                res = env->driver_lib_loader->CallFunction<RDS_SQLEndTran>(AS_RDS_STR("SQLEndTran"),
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
                    res = env->driver_lib_loader->CallFunction<RDS_SQLEndTran>(AS_RDS_STR("SQLEndTran"),
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

SQLRETURN RDS_FreeConnect_Impl(
    SQLHDBC        ConnectionHandle)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    bool has_free_stmt_errors = false, has_free_desc_errors = false;

    // Remove connection from environment
    env->dbc_list.remove(dbc); // TODO - Make this into a function within ENV to make use of locks

    // Clean up wrapped DBC
    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLFreeHandle>(AS_RDS_STR("SQLFreeHandle"),
        SQL_HANDLE_DBC, dbc->wrapped_dbc
    );
    RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);

    if (dbc->plugin_head) delete dbc->plugin_head;
    if (dbc->err) delete dbc->err;

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

    // Clean underlying Descriptors
    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLFreeHandle>(AS_RDS_STR("SQLFreeHandle"),
        SQL_HANDLE_DESC, desc->wrapped_desc
    );
    RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);

    if (desc->err) delete desc->err;

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

    if (env->driver_lib_loader) {
        // Clean underlying Env
        RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLFreeHandle>(AS_RDS_STR("SQLFreeHandle"),
            SQL_HANDLE_ENV, env->wrapped_env
        );
        RDS_ProcessLibRes(SQL_HANDLE_ENV, env, res);
        env->driver_lib_loader.reset();
    }

    if (env->err) delete env->err;

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

    // Clean underlying Statements
    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLFreeHandle>(AS_RDS_STR("SQLFreeHandle"),
        SQL_HANDLE_STMT, stmt->wrapped_stmt
    );
    RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);

    if (stmt->err) delete stmt->err;

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
    if (dbc->wrapped_dbc && env->driver_lib_loader) {
        RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetConnectAttr>(AS_RDS_STR("SQLGetConnectAttr"),
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

    SQLRETURN ret = SQL_ERROR;

    // If already connected, apply value to underlying DBC, otherwise track and apply on connect
    if (dbc->wrapped_dbc && env->driver_lib_loader) {
        RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetConnectAttr>(AS_RDS_STR("SQLSetConnectAttr"),
            dbc->wrapped_dbc, Attribute, ValuePtr, StringLength
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
    } else {
        dbc->attr_map.insert_or_assign(Attribute, std::make_pair(ValuePtr, StringLength));
    }

    return ret;
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
            return SQL_ERROR;
        }
    } else {
        // TODO - No underlying driver in ConnStr
        //   set error and return?
        // OR
        //   check if ENV has an underlying driver already
        NOT_IMPLEMENTED;
    }

    RdsLibResult res;
    SQLRETURN ret = SQL_SUCCESS;

    // Initialize Wrapped ENV
    // Create Wrapped HENV for Wrapped HDBC if not already allocated
    if (!env->wrapped_env) {
        res = env->driver_lib_loader->CallFunction<RDS_SQLAllocHandle>(AS_RDS_STR("SQLAllocHandle"),
            SQL_HANDLE_ENV, nullptr, &env->wrapped_env
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
        // Apply Tracked Environment Attributes
        for (auto const& [key, val] : env->attr_map) {
            res = env->driver_lib_loader->CallFunction<RDS_SQLSetEnvAttr>(AS_RDS_STR("SQLSetEnvAttr"),
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
                    case AuthType::PASSWORD:
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

// GUI Related
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLBindCol>(AS_RDS_STR("SQLBindCol"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLBindParameter>(AS_RDS_STR("SQLBindParameter"),
        stmt->wrapped_stmt, ParameterNumber, InputOutputType, ValueType, ParameterType, ColumnSize, DecimalDigits, ParameterValuePtr, BufferLength, StrLen_or_IndPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLBulkOperations>(AS_RDS_STR("SQLBulkOperations"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLCancel>(AS_RDS_STR("SQLCancel"),
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

                res = env->driver_lib_loader->CallFunction<RDS_SQLCancel>(AS_RDS_STR("SQLCancel"),
                    stmt->wrapped_stmt
                );
                ret = RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLCloseCursor>(AS_RDS_STR("SQLCloseCursor"),
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLColAttribute>(AS_RDS_STR("SQLColAttribute"),
        stmt->wrapped_stmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLColAttributes>(AS_RDS_STR("SQLColAttributes"),
        stmt->wrapped_stmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLColumnPrivileges>(AS_RDS_STR("SQLColumnPrivileges"),
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, ColumnName, NameLength4
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLColumns>(AS_RDS_STR("SQLColumns"),
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, ColumnName, NameLength4
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = src_env->driver_lib_loader->CallFunction<RDS_SQLCopyDesc>(AS_RDS_STR("SQLCopyDesc"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLDescribeCol>(AS_RDS_STR("SQLDescribeCol"),
        stmt->wrapped_stmt, ColumnNumber, ColumnName, BufferLength, NameLengthPtr, DataTypePtr, ColumnSizePtr, DecimalDigitsPtr, NullablePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLDescribeParam>(AS_RDS_STR("SQLDescribeParam"),
        stmt->wrapped_stmt, ParameterNumber, DataTypePtr, ParameterSizePtr, DecimalDigitsPtr, NullablePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLDisconnect(
    SQLHDBC        ConnectionHandle)
{
    NULL_CHECK_ENV_ACCESS_DBC(ConnectionHandle);
    DBC *dbc = (DBC*) ConnectionHandle;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLDisconnect>(AS_RDS_STR("SQLDisconnect"),
        dbc->wrapped_dbc
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
}

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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLExecDirect>(AS_RDS_STR("SQLExecDirect"),
        stmt->wrapped_stmt, StatementText, TextLength
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
}

SQLRETURN SQL_API SQLExecute(
    SQLHSTMT       StatementHandle)
{
    NULL_CHECK_ENV_ACCESS_STMT(StatementHandle);
    STMT *stmt = (STMT*) StatementHandle;
    DBC *dbc = (DBC*) stmt->dbc;
    ENV *env = (ENV*) dbc->env;

    std::lock_guard<std::recursive_mutex> lock_guard(stmt->lock);

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLExecute>(AS_RDS_STR("SQLExecute"),
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLExtendedFetch>(AS_RDS_STR("SQLExtendedFetch"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLFetch>(AS_RDS_STR("SQLFetch"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLFetchScroll>(AS_RDS_STR("SQLFetchScroll"),
        stmt->wrapped_stmt, FetchOrientation, FetchOffset
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLForeignKeys>(AS_RDS_STR("SQLForeignKeys"),
        stmt->wrapped_stmt, PKCatalogName, NameLength1, PKSchemaName, NameLength2, PKTableName, NameLength3, FKCatalogName, NameLength4, FKSchemaName, NameLength5, FKTableName, NameLength6
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetCursorName>(AS_RDS_STR("SQLGetCursorName"),
        stmt->wrapped_stmt, CursorName, BufferLength, NameLengthPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetData>(AS_RDS_STR("SQLGetData"),
        stmt->wrapped_stmt, Col_or_Param_Num, TargetType, TargetValuePtr, BufferLength, StrLen_or_IndPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetDescField>(AS_RDS_STR("SQLGetDescField"),
        desc->wrapped_desc, RecNumber, FieldIdentifier, ValuePtr, BufferLength, StringLengthPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetDescRec>(AS_RDS_STR("SQLGetDescRec"),
        desc->wrapped_desc, RecNumber, Name, BufferLength, StringLengthPtr, TypePtr, SubTypePtr, LengthPtr, PrecisionPtr, ScalePtr, NullablePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
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
                if (env->driver_lib_loader && env->wrapped_env) {
                    has_underlying_driver_alloc = true;
                    res = env->driver_lib_loader->CallFunction<RDS_SQLGetDiagField>(AS_RDS_STR("SQLGetDiagField"),
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

                if (env->driver_lib_loader && dbc->wrapped_dbc) {
                    has_underlying_driver_alloc = true;
                    res = env->driver_lib_loader->CallFunction<RDS_SQLGetDiagField>(AS_RDS_STR("SQLGetDiagField"),
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

                if (env->driver_lib_loader && stmt->wrapped_stmt) {
                    has_underlying_driver_alloc = true;
                    res = env->driver_lib_loader->CallFunction<RDS_SQLGetDiagField>(AS_RDS_STR("SQLGetDiagField"),
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

                if (env->driver_lib_loader && desc->wrapped_desc) {
                    has_underlying_driver_alloc = true;
                    res = env->driver_lib_loader->CallFunction<RDS_SQLGetDiagField>(AS_RDS_STR("SQLGetDiagField"),
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
    RdsLibResult res;
    SQLRETURN ret = SQL_ERROR;
    SQLULEN len = 0, value = 0;
    const char *char_value = nullptr;
    ERR_INFO *err = nullptr;
    bool has_underlying_driver_alloc = false;

    switch (HandleType) {
        case SQL_HANDLE_ENV:
            NULL_CHECK_HANDLE(Handle);
            {
                env = (ENV*) Handle;

                std::lock_guard<std::recursive_mutex> lock_guard(env->lock);

                if (env->driver_lib_loader && env->wrapped_env) {
                    has_underlying_driver_alloc = true;
                    res = env->driver_lib_loader->CallFunction<RDS_SQLGetDiagRec>(AS_RDS_STR("SQLGetDiagRec"),
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

                if (env->driver_lib_loader && dbc->wrapped_dbc) {
                    has_underlying_driver_alloc = true;
                    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetDiagRec>(AS_RDS_STR("SQLGetDiagRec"),
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

                if (env->driver_lib_loader && stmt->wrapped_stmt) {
                    has_underlying_driver_alloc = true;
                    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetDiagRec>(AS_RDS_STR("SQLGetDiagRec"),
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

                if (env->driver_lib_loader && desc->wrapped_desc) {
                    has_underlying_driver_alloc = true;
                    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetDiagRec>(AS_RDS_STR("SQLGetDiagRec"),
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
    SQLRETURN ret = SQL_SUCCESS;

    std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);

    // Query underlying driver if connection is established
    if (dbc && dbc->wrapped_dbc) {
        ENV* env = (ENV*) dbc->env;
        RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetFunctions>(AS_RDS_STR("SQLGetFunctions"),
            dbc->wrapped_dbc, FunctionId, SupportedPtr
        );
        ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
    }

    // TODO - THIS IS HARDCODED
    // Will need to keep track of a map of the current implemented functions
    // as ODBC adds/removes functions in the future
    return ret;
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
            RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetInfo>(AS_RDS_STR("SQLGetInfo"),
                dbc->wrapped_dbc, InfoType, InfoValuePtr, BufferLength, StringLengthPtr
            );
            return RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetStmtAttr>(AS_RDS_STR("SQLGetStmtAttr"),
        stmt->wrapped_stmt, Attribute, ValuePtr, BufferLength, StringLengthPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetStmtOption>(AS_RDS_STR("SQLGetStmtOption"),
        stmt->wrapped_stmt, Attribute, ValuePtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLGetTypeInfo>(AS_RDS_STR("SQLGetTypeInfo"),
        stmt->wrapped_stmt, DataType
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLMoreResults>(AS_RDS_STR("SQLMoreResults"),
        stmt->wrapped_stmt
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLNativeSql>(AS_RDS_STR("SQLNativeSql"),
        dbc->wrapped_dbc, InStatementText, TextLength1, OutStatementText, BufferLength, TextLength2Ptr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DBC, dbc, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLNumParams>(AS_RDS_STR("SQLNumParams"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLNumResultCols>(AS_RDS_STR("SQLNumResultCols"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLParamData>(AS_RDS_STR("SQLParamData"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLParamOptions>(AS_RDS_STR("SQLParamOptions"),
        stmt->wrapped_stmt, Crow, FetchOffsetPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLPrepare>(AS_RDS_STR("SQLPrepare"),
        stmt->wrapped_stmt, StatementText, TextLength
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLPrimaryKeys>(AS_RDS_STR("SQLPrimaryKeys"),
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLProcedureColumns>(AS_RDS_STR("SQLProcedureColumns"),
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, ProcName, NameLength3, ColumnName, NameLength4
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLProcedures>(AS_RDS_STR("SQLProcedures"),
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, ProcName, NameLength3
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLPutData>(AS_RDS_STR("SQLPutData"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLRowCount>(AS_RDS_STR("SQLRowCount"),
        stmt->wrapped_stmt, RowCountPtr
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetCursorName>(AS_RDS_STR("SQLSetCursorName"),
        stmt->wrapped_stmt, CursorName, NameLength
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetDescField>(AS_RDS_STR("SQLSetDescField"),
        desc->wrapped_desc, RecNumber, FieldIdentifier, ValuePtr, BufferLength
    );
    return RDS_ProcessLibRes(SQL_HANDLE_DESC, desc, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetDescRec>(AS_RDS_STR("SQLSetDescRec"),
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

    // Update existing connections environments
    if (env->driver_lib_loader) {
        RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetEnvAttr>(AS_RDS_STR("SQLSetEnvAttr"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetParam>(AS_RDS_STR("SQLSetParam"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetPos>(AS_RDS_STR("SQLSetPos"),
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetScrollOptions>(AS_RDS_STR("SQLSetScrollOptions"),
        stmt->wrapped_stmt, Concurrency, KeysetSize, RowsetSize
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetStmtAttr>(AS_RDS_STR("SQLSetStmtAttr"),
        stmt->wrapped_stmt, Attribute, ValuePtr, StringLength
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSetStmtOption>(AS_RDS_STR("SQLSetStmtOption"),
        stmt->wrapped_stmt, Option, Param
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLSpecialColumns>(AS_RDS_STR("SQLSpecialColumns"),
        stmt->wrapped_stmt, IdentifierType, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, Scope, Nullable
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLStatistics>(AS_RDS_STR("SQLStatistics"),
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, Unique, Reserved
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLTablePrivileges>(AS_RDS_STR("SQLTablePrivileges"),
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3
    );
    return RDS_ProcessLibRes(SQL_HANDLE_STMT, stmt, res);
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

    RdsLibResult res = env->driver_lib_loader->CallFunction<RDS_SQLTables>(AS_RDS_STR("SQLTables"),
        stmt->wrapped_stmt, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, TableType, NameLength4
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

    return RDS_SQLEndTran_Impl(
        ConnectionHandle ? SQL_HANDLE_DBC : SQL_HANDLE_ENV,
        ConnectionHandle ? ConnectionHandle : EnvironmentHandle,
        CompletionType
    );
}
