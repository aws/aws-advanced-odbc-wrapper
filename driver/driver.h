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
#include <sqlext.h>
#include <sqltypes.h>
#include <odbcinst.h>

#include <list>
#include <map>
#include <mutex>
#include <set>

#include "error.h"

#include "plugin/base_plugin.h"
#include "util/rds_lib_loader.h"
#include "util/rds_strings.h"
#include "util/topology_service.h"

/* Const Lengths */
#define NO_DATA_SQL_STATE     "00000"
#define NO_DATA_NATIVE_ERR    0
#define MAX_SQL_STATE_LEN     6
#define ODBC_VER_SiZE         16

/* Struct Declarations */
typedef enum
{
    CONN_NOT_CONNECTED,
    CONN_CONNECTED,
    CONN_DOWN,
    CONN_EXECUTING
} CONN_STATUS;

typedef enum
{
    TRANSACTION_CLOSED,
    TRANSACTION_OPEN,
    TRANSACTION_ERROR
} TRANSACTION_STATUS;

struct ERR_INFO;
struct ENV;
struct DBC;
struct STMT;
struct DESC;

class BasePlugin;

/* Structures */
struct ENV {
    std::recursive_mutex        lock;
    std::list<DBC*>             dbc_list;
    // TODO - May need to change SQLPOINTER to an actual object
    std::map<SQLINTEGER, std::pair<SQLPOINTER, SQLINTEGER>> attr_map; // Key, <Value, Length>
    ERR_INFO*                   err = nullptr;
    char                        sql_error_called = 0;

    SQLHENV                     wrapped_env;

    std::shared_ptr<RdsLibLoader> driver_lib_loader;

     ~ENV() {
          if (err) {
               delete err;
          }
          err = nullptr;
     }
}; // ENV

struct DBC {
    std::recursive_mutex        lock;
    ENV*                        env;
    std::list<STMT*>            stmt_list;
    uint16_t                    unnamed_cursor_count;
    std::list<DESC*>            desc_list;
    SQLHDBC                     wrapped_dbc;
    CONN_STATUS                 conn_status;
    TRANSACTION_STATUS          transaction_status;
    bool                        auto_commit = true; // By default, drivers will be in auto commit mode

    // TODO - May need to change SQLPOINTER to an actual object
    std::map<SQLINTEGER, std::pair<SQLPOINTER, SQLINTEGER>> attr_map; // Key, <Value, Length>

    // Connection Information, i.e. Server, Port, UID, Pass, Plugin Info, etc
    std::map<std::string, std::string>  conn_attr; // Key, Value
    BasePlugin*                         plugin_head = nullptr;
    std::shared_ptr<TopologyService>    topology_service;
    ERR_INFO*                           err = nullptr;
    char                                sql_error_called = 0;

     ~DBC() {
          if (plugin_head) {
               delete plugin_head;
          }
          if (err) {
               delete err;
          }
          plugin_head = nullptr;
          err = nullptr;
     }
}; // DBC

struct STMT {
    // TODO - Do we need lock?
    std::recursive_mutex        lock;
    DBC*                        dbc;
    SQLHSTMT                    wrapped_stmt;

    DESC*                       app_row_desc    = SQL_NULL_HANDLE;
    DESC*                       app_param_desc  = SQL_NULL_HANDLE;
    DESC*                       imp_row_desc    = SQL_NULL_HANDLE;
    DESC*                       imp_param_desc  = SQL_NULL_HANDLE;

    // TODO - May need to change SQLPOINTER to an actual object
    std::map<SQLINTEGER, std::pair<SQLPOINTER, SQLINTEGER>> attr_map; // Key, <Value, Length>
    std::string cursor_name;

    ERR_INFO*                   err = nullptr;
    char                        sql_error_called = 0;

     ~STMT() {
          if (err) {
               delete err;
          }
          err = nullptr;
     }
}; // STMT

struct DESC {
    // TODO - Do we need lock?
    std::recursive_mutex        lock;
    // TODO - What to put here
    DBC*                        dbc;
    SQLHDESC                    wrapped_desc;
    ERR_INFO*                   err = nullptr;
    char                        sql_error_called = 0;

     ~DESC() {
          if (err) {
               delete err;
          }
          err = nullptr;
     }
}; // DESC

/* Function Declarations */
SQLRETURN RDS_AllocEnv(
     SQLHENV *      EnvironmentHandlePointer);

SQLRETURN RDS_AllocDbc(
     SQLHENV         EnvironmentHandle,
     SQLHDBC *       ConnectionHandlePointer);

SQLRETURN RDS_AllocStmt(
     SQLHDBC        ConnectionHandle,
     SQLHSTMT *     StatementHandlePointer);

SQLRETURN RDS_AllocDesc(
     SQLHDBC        ConnectionHandle,
     SQLHANDLE *    DescriptorHandlePointer);

SQLRETURN RDS_SQLEndTran(
     SQLSMALLINT    HandleType,
     SQLHANDLE      Handle,
     SQLSMALLINT    CompletionType);

SQLRETURN RDS_FreeConnect(
     SQLHDBC        ConnectionHandle);

SQLRETURN RDS_FreeDesc(
     SQLHDESC       DescriptorHandle);

SQLRETURN RDS_FreeEnv(
     SQLHENV        EnvironmentHandle);

SQLRETURN RDS_FreeStmt(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   Option);

SQLRETURN RDS_GetConnectAttr(
     SQLHDBC        ConnectionHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     BufferLength,
     SQLINTEGER *   StringLengthPtr);

SQLRETURN RDS_SQLSetConnectAttr(
     SQLHDBC        ConnectionHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     StringLength);

/* Simple Macros */
#define NOT_IMPLEMENTED \
     return SQL_ERROR

#define NULL_CHECK_HANDLE(h)                           \
    if (h == NULL) return SQL_INVALID_HANDLE
#define NULL_CHECK_ENV(h)                              \
     if (h == NULL                                     \
          || ((ENV*) h)== NULL)                  \
          return SQL_INVALID_HANDLE
#define NULL_CHECK_ENV_ACCESS_DBC(h)                   \
    if (h == NULL                                      \
         || ((DBC*) h)->env == NULL)                   \
         return SQL_INVALID_HANDLE
#define NULL_CHECK_ENV_ACCESS_STMT(h)                  \
    if (h == NULL                                      \
         || ((STMT*) h)->dbc == NULL                   \
         || ((DBC*) ((STMT*) h)->dbc)->env == NULL)    \
         return SQL_INVALID_HANDLE
#define NULL_CHECK_ENV_ACCESS_DESC(h)                  \
    if (h == NULL                                      \
         || ((DESC*) h)->dbc == NULL                   \
         || ((DBC*) ((DESC*) h)->dbc)->env == NULL)    \
         return SQL_INVALID_HANDLE

#define NULL_CHECK_CALL_LIB_FUNC(lib_loader, fn_type, fn_name, ...) lib_loader ? \
    lib_loader->CallFunction<fn_type>(fn_name, __VA_ARGS__) \
    : RdsLibResult{.fn_load_success = false, .fn_result = SQL_ERROR}

#define CHECK_WRAPPED_ENV(h)                      \
    if (h == NULL                                 \
         || ((ENV*) h)->wrapped_env == NULL)      \
         return SQL_INVALID_HANDLE
#define CHECK_WRAPPED_DBC(h)                      \
    if (h == NULL                                 \
         || ((DBC*) h)->wrapped_dbc == NULL)      \
         return SQL_INVALID_HANDLE
#define CHECK_WRAPPED_STMT(h)                     \
    if (h == NULL                                 \
         || ((STMT*) h)->wrapped_stmt == NULL)    \
         return SQL_INVALID_HANDLE
#define CHECK_WRAPPED_DESC(h)                     \
    if (h == NULL                                 \
         || ((DESC*) h)->wrapped_desc == NULL)    \
         return SQL_INVALID_HANDLE

#define CLEAR_ENV_ERROR(env)                      \
     do {                                         \
          if (env) {                              \
               ((ENV*)env)->sql_error_called = 0; \
               delete ((ENV*)env)->err;           \
               ((ENV*)env)->err = nullptr;        \
          }                                       \
     } while (0)
#define CLEAR_DBC_ERROR(dbc)                      \
     do {                                         \
          if (dbc) {                              \
               ((DBC*)dbc)->sql_error_called = 0; \
               delete ((DBC*)dbc)->err;           \
               ((DBC*)dbc)->err = nullptr;        \
          }                                       \
     } while (0)
#define CLEAR_STMT_ERROR(stmt)                         \
     do {                                              \
          if (stmt) {                                  \
               ((STMT*)stmt)->sql_error_called = 0;    \
               delete ((STMT*)stmt)->err;              \
               ((STMT*)stmt)->err = nullptr;           \
          }                                            \
     } while (0)
#define CLEAR_DESC_ERROR(desc)                         \
     do {                                              \
          if (desc) {                                  \
               ((DESC*)desc)->sql_error_called = 0;    \
               delete ((DESC*)desc)->err;              \
               ((DESC*)desc)->err = nullptr;           \
          }                                            \
     } while (0)

#define NEXT_ERROR(err) err ? 0 : (err = 1)

#define NEXT_ENV_ERROR(env)   \
     env ? NEXT_ERROR(((ENV*)env)->sql_error_called) : 0
#define NEXT_DBC_ERROR(dbc)   \
     dbc ? NEXT_ERROR(((DBC*)dbc)->sql_error_called) : 0
#define NEXT_STMT_ERROR(stmt) \
     stmt ? NEXT_ERROR(((STMT*)stmt)->sql_error_called) : 0
#define NEXT_DESC_ERROR(desc) \
     desc ? NEXT_ERROR(((DESC*)desc)->sql_error_called) : 0

#endif // DRIVER_H
