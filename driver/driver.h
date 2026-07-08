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

#ifndef DRIVER_H_
#define DRIVER_H_

#ifndef ODBCVER
#define ODBCVER 0x0380
#endif

#define ODBCVER_BITS 256

#define DRIVER_VERSION "1.0.0"

#if WIN32
#ifdef UNICODE
#define DRIVER_NAME "aws-advanced-odbc-wrapper-w.dll"
#else
#define DRIVER_NAME "aws-advanced-odbc-wrapper-a.dll"
#endif
#elif __APPLE__
#ifdef UNICODE
#define DRIVER_NAME "aws-advanced-odbc-wrapper-w.dylib"
#else
#define DRIVER_NAME "aws-advanced-odbc-wrapper-a.dylib"
#endif
#elif __linux__ || __unix__
#ifdef UNICODE
#define DRIVER_NAME "aws-advanced-odbc-wrapper-w.so"
#else
#define DRIVER_NAME "aws-advanced-odbc-wrapper-a.so"
#endif
#else
#ifdef UNICODE
#define DRIVER_NAME "aws-advanced-odbc-wrapper-w"
#else
#define DRIVER_NAME "aws-advanced-odbc-wrapper-a"
#endif
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include <sql.h>

#include <atomic>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include "error.h"

/* Forward Declarations */
struct ENV;
struct DBC;
struct STMT;
struct DESC;

struct RdsLibResult;

class BasePlugin;
class RdsLibLoader;
class LoggerWrapper;
class PluginService;

/* Const Lengths */
#define NO_DATA_SQL_STATE "00000"
#define NO_DATA_NATIVE_ERR 0
#define MAX_SQL_STATE_LEN 6
#define ODBC_VER_SiZE 16
#define MAX_MSG_LEN 1024

/* Struct Declarations */
typedef enum { CONN_NOT_CONNECTED, CONN_CONNECTED, CONN_DOWN, CONN_EXECUTING } CONN_STATUS;

typedef enum { TRANSACTION_CLOSED, TRANSACTION_OPEN, TRANSACTION_ERROR } TRANSACTION_STATUS;

/* Structures */
struct ENV {
    std::recursive_mutex lock;
    std::list<DBC*> dbc_list;
    // TODO - May need to change SQLPOINTER to an actual object
    std::map<SQLINTEGER, std::pair<SQLPOINTER, SQLINTEGER>> attr_map;  // Key, <Value, Length>
    std::unique_ptr<ERR_INFO> err;
    char sql_error_called = 0;
    std::shared_ptr<LoggerWrapper> logger_wrapper;

    SQLHENV wrapped_env;

    std::shared_ptr<RdsLibLoader> driver_lib_loader;
    std::atomic<bool> use_4_bytes_user_app = false;

    ~ENV();
};  // ENV

struct DBC {
    std::recursive_mutex lock;
    ENV* env;
    std::list<STMT*> stmt_list;
    uint16_t unnamed_cursor_count;
    std::list<DESC*> desc_list;
    SQLHDBC wrapped_dbc = SQL_NULL_HDBC;
    CONN_STATUS conn_status;
    TRANSACTION_STATUS transaction_status;
    bool auto_commit = true;  // By default, drivers will be in auto commit mode

    // TODO - May need to change SQLPOINTER to an actual object
    std::map<SQLINTEGER, std::pair<SQLPOINTER, SQLINTEGER>> attr_map;  // Key, <Value, Length>

    // Connection Information, i.e. Server, Port, UID, Pass, Plugin Info, etc
    std::map<std::string, std::string> conn_attr;  // Key, Value
    bool allow_interactive_auth = false;
    BasePlugin* plugin_head = nullptr;
    std::shared_ptr<PluginService> plugin_service;
    std::unique_ptr<ERR_INFO> err;
    char sql_error_called = 0;

    ~DBC();
};  // DBC

// Tracks SQLBindCol WCHAR binding for 2-byte/4-byte conversion.
struct BoundColBuffer {
    SQLUSMALLINT            column_number;
    SQLPOINTER              app_ptr;            // User's TargetValuePtr
    SQLLEN                  app_buf_len;        // User's BufferLength
    SQLLEN*                 app_str_len_ptr;    // User's StrLen_or_IndPtr
    std::vector<SQLTCHAR>   local_buf;          // Wrapper buffer passed to underlying driver
    // Wrapper StrLen_or_Ind, heap-safe w/ shared_ptr
    std::shared_ptr<SQLLEN> local_str_len = std::make_shared<SQLLEN>(0);

};

// Tracks SQLBindParameter WCHAR binding for 2-byte/4-byte conversion.
struct BoundParamBuffer {
    SQLUSMALLINT            param_number;
    SQLSMALLINT             input_output_type;  // SQL_PARAM_INPUT, SQL_PARAM_OUTPUT, SQL_PARAM_INPUT_OUTPUT
    SQLSMALLINT             value_type;
    SQLSMALLINT             param_type;
    SQLULEN                 column_size;
    SQLSMALLINT             decimal_digits;
    SQLPOINTER              app_ptr;            // User's ParameterValuePtr
    SQLLEN                  app_buf_len;        // User's BufferLength
    SQLLEN*                 app_str_len_ptr;    // User's StrLen_or_IndPtr
    std::vector<SQLTCHAR>   local_buf;          // Wrapper buffer passed to underlying driver
    // Wrapper StrLen_or_Ind, heap-safe w/ shared_ptr
    std::shared_ptr<SQLLEN> local_str_len = std::make_shared<SQLLEN>(0);
};

struct STMT {
    // TODO - Do we need lock?
    std::recursive_mutex lock;
    DBC* dbc;
    SQLHSTMT wrapped_stmt;

    DESC* app_row_desc = SQL_NULL_HANDLE;
    DESC* app_param_desc = SQL_NULL_HANDLE;
    DESC* imp_row_desc = SQL_NULL_HANDLE;
    DESC* imp_param_desc = SQL_NULL_HANDLE;

    // TODO - May need to change SQLPOINTER to an actual object
    std::map<SQLINTEGER, std::pair<SQLPOINTER, SQLINTEGER>> attr_map;  // Key, <Value, Length>
    std::string cursor_name;

    // Buffers for UTF32 support
    std::vector<BoundColBuffer> bound_col_buffers;      // Intercepted WCHAR column bindings
    std::vector<BoundParamBuffer> bound_param_buffers;  // Intercepted WCHAR param bindings
    bool put_data_char_conversion = false;

    std::unique_ptr<ERR_INFO> err;
    char sql_error_called = 0;

    ~STMT();
};  // STMT

struct DESC {
    // TODO - Do we need lock?
    std::recursive_mutex lock;
    // TODO - What to put here
    DBC* dbc;
    SQLHDESC wrapped_desc;
    std::unique_ptr<ERR_INFO> err;
    char sql_error_called = 0;

    ~DESC();
};  // DESC

/* Function Declarations */
SQLRETURN RDS_AllocEnv(SQLHENV* EnvironmentHandlePointer);

SQLRETURN RDS_AllocDbc(SQLHENV EnvironmentHandle, SQLHDBC* ConnectionHandlePointer);

SQLRETURN RDS_AllocStmt(SQLHDBC ConnectionHandle, SQLHSTMT* StatementHandlePointer);

SQLRETURN RDS_AllocDesc(SQLHDBC ConnectionHandle, SQLHANDLE* DescriptorHandlePointer);

SQLRETURN RDS_SQLEndTran(SQLSMALLINT HandleType, SQLHANDLE Handle, SQLSMALLINT CompletionType);

SQLRETURN RDS_FreeConnect(SQLHDBC ConnectionHandle);

SQLRETURN RDS_FreeDesc(SQLHDESC DescriptorHandle);

SQLRETURN RDS_FreeEnv(SQLHENV EnvironmentHandle);

SQLRETURN RDS_FreeStmt(SQLHSTMT StatementHandle, SQLUSMALLINT Option);

SQLRETURN RDS_GetConnectAttr(SQLHDBC ConnectionHandle, SQLINTEGER Attribute, SQLPOINTER ValuePtr, SQLINTEGER BufferLength,
                             SQLINTEGER* StringLengthPtr);

SQLRETURN RDS_SQLSetConnectAttr(SQLHDBC ConnectionHandle, SQLINTEGER Attribute, SQLPOINTER ValuePtr, SQLINTEGER StringLength);

/* Simple Macros */
#define RDS_NOT_IMPLEMENTED return SQL_ERROR

#define NULL_CHECK_CALL_LIB_FUNC(lib_loader, fn_type, fn_name, ...)                       \
    lib_loader ? lib_loader->CallFunction<fn_type>(fn_name, __VA_ARGS__) : RdsLibResult { \
        .fn_load_success = false, .fn_result = SQL_ERROR                                  \
    }

/* Handle Helpers */

// Callers must return SQL_INVALID_HANDLE when this fails.
template <typename HandleT>
bool HasEnvAccess(SQLHANDLE handle) {
    const HandleT* typed = static_cast<const HandleT*>(handle);
    if constexpr (std::is_same_v<HandleT, ENV>) {
        return typed != nullptr;
    } else if constexpr (std::is_same_v<HandleT, DBC>) {
        return typed != nullptr && typed->env != nullptr;
    } else {
        return typed != nullptr && typed->dbc != nullptr && typed->dbc->env != nullptr;
    }
}

// Callers must return SQL_INVALID_HANDLE when this fails.
inline bool HasWrappedHandle(const ENV* env) { return env != nullptr && env->wrapped_env != nullptr; }
inline bool HasWrappedHandle(const DBC* dbc) { return dbc != nullptr && dbc->wrapped_dbc != nullptr; }
inline bool HasWrappedHandle(const STMT* stmt) { return stmt != nullptr && stmt->wrapped_stmt != nullptr; }
inline bool HasWrappedHandle(const DESC* desc) { return desc != nullptr && desc->wrapped_desc != nullptr; }

// Releases the handle's error record and re-arms the SQLError one-shot flag.
template <typename HandleT>
void ClearError(HandleT* handle) {
    if (handle != nullptr) {
        handle->sql_error_called = 0;
        handle->err.reset();
    }
}

// Diagnostic record number for a direct SQLError call:
// 1 for the first call on the handle, 0 (no more data) afterwards.
// UnixODBC will enter an infinite loop if errors repeat indefinitely.
template <typename HandleT>
SQLSMALLINT NextErrorRecord(SQLHANDLE handle) {
    HandleT* typed = static_cast<HandleT*>(handle);
    if (typed == nullptr || typed->sql_error_called != 0) {
        return 0;
    }
    typed->sql_error_called = 1;
    return 1;
}

#endif  // DRIVER_H
