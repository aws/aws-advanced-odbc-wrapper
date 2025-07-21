#ifndef DRIVER_H_
#define DRIVER_H_

#ifndef ODBCVER
    #define ODBCVER 0x0380
#endif

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#include <list>
#include <map>
#include <mutex>
#include <set>

#include "base_plugin.h"
#include "rds_lib_loader.h"
#include "rds_strings.h"

class BasePlugin;

/* Const Lengths */
#define NO_DATA_SQL_STATE     TEXT("00000")
#define NO_DATA_NATIVE_ERR    0
#define MAX_SQL_STATE_LEN     5

/* Struct Declarations */
typedef enum
{
     CONN_NOT_CONNECTED,
     CONN_CONNECTED,
     CONN_DOWN,
     CONN_EXECUTING
} CONN_STATUS;

struct ERR_INFO;
struct ENV;
struct DBC;
struct STMT;
struct DESC;

/* Structures */
struct ERR_INFO {
    char*               error_msg;
    signed char         error_num;
    char*               sqlstate;
}; // ERR_INFO

struct ENV {
    std::recursive_mutex      lock;
    std::list<DBC*>           dbc_list;
    // TODO - May need to change SQLPOINTER to an actual object
    std::map<SQLINTEGER, std::pair<SQLPOINTER, SQLINTEGER>> attr_map; // Key, <Value, Length>

    // Error Info, to be used if no underlying ENV
    ERR_INFO* err;

    // TODO - Alternative, store wrapped ENV & Module Handle here
    //    Module Handle stored in ENV allows it to be shared and
    //    reduce the amount of times it needs to be tracked / "loaded"
    //    Downside, for Statement to get access, it must go through
    //    Stmt -> Dbc -> Env -> Module Handle
    //    ^ Can also store pointer in each structure to refer to this module
    //    Can still have multiple DBCs per ENV
    //    but need to enforce that the underlying driver is the same
    RDS_STR                   wrapped_driver_path;
    // TODO - Wrap the driver handle & function map into a class to be passed around
    MODULE_HANDLE             wrapped_driver_handle;
    SQLHENV                   wrapped_env;
    // TODO - Thread safety? Multiple can read, but when adding,
    //    will having multiple threads put to the same key
    //    with the same value cause any negative effects?
    std::map<RDS_STR, MODULE_HANDLE> wrapped_func;
}; // ENV

struct DBC {
    std::recursive_mutex      lock;
    ENV*                      env;
    std::list<STMT*>          stmt_list;
    std::list<DESC*>          desc_list;
    SQLHDBC                   wrapped_dbc;
    CONN_STATUS               conn_status;

    // TODO - May need to change SQLPOINTER to an actual object
    std::map<SQLINTEGER, std::pair<SQLPOINTER, SQLINTEGER>> attr_map; // Key, <Value, Length>

    // Connection Information, i.e. Server, Port, UID, Pass, Plugin Info, etc
    std::map<RDS_STR, RDS_STR> conn_attr; // Key, Value
    BasePlugin*         plugin_head;

    // Error Info, to be used if no underlying DBC
    ERR_INFO* err;
}; // DBC

struct STMT {
    // TODO - Do we need lock?
    std::recursive_mutex lock;
    DBC*                dbc;
    SQLHSTMT            wrapped_stmt;

    // Error Info, to be used if no underlying STMT
    ERR_INFO* err;
}; // STMT

struct DESC {
    // TODO - Do we need lock?
    std::recursive_mutex lock;
    // TODO - What to put here
    DBC*                dbc;
    SQLHDESC            wrapped_desc;

    // Error Info, to be used if no underlying DESC
    ERR_INFO* err;
}; // DESC

/* Function Declarations */
SQLRETURN RDS_AllocEnv_Impl(
     SQLHENV *      EnvironmentHandlePointer);

SQLRETURN RDS_AllocDbc_Impl(
     SQLHENV         EnvironmentHandle,
     SQLHDBC *       ConnectionHandlePointer);

SQLRETURN RDS_AllocStmt_Impl(
     SQLHDBC        ConnectionHandle,
     SQLHSTMT *     StatementHandlePointer);

SQLRETURN RDS_AllocDesc_Impl(
     SQLHDBC        ConnectionHandle,
     SQLHANDLE *    DescriptorHandlePointer);

SQLRETURN RDS_SQLEndTran_Impl(
     SQLSMALLINT    HandleType,
     SQLHANDLE      Handle,
     SQLSMALLINT    CompletionType);

SQLRETURN RDS_FreeConnect_Impl(
     SQLHDBC        ConnectionHandle);

SQLRETURN RDS_FreeDesc_Impl(
     SQLHDESC       DescriptorHandle);

SQLRETURN RDS_FreeEnv_Impl(
     SQLHENV        EnvironmentHandle);

SQLRETURN RDS_FreeStmt_Impl(
     SQLHSTMT       StatementHandle);

SQLRETURN RDS_GetConnectAttr_Impl(
     SQLHDBC        ConnectionHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     BufferLength,
     SQLINTEGER *   StringLengthPtr);

SQLRETURN RDS_SQLSetConnectAttr_Impl(
     SQLHDBC        ConnectionHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     StringLength);

/* Simple Macros */
#define NOT_IMPLEMENTED \
     return SQL_ERROR

#define NULL_CHECK_HANDLE(h) \
    if (h == NULL) return SQL_INVALID_HANDLE
#define NULL_CHECK_ENV_ACCESS_DBC(h) \
    if (h == NULL \
         || ((DBC*) h)->env == NULL) \
         return SQL_INVALID_HANDLE
#define NULL_CHECK_ENV_ACCESS_STMT(h) \
    if (h == NULL \
         || ((STMT*) h)->dbc == NULL \
         || ((DBC*) ((STMT*) h)->dbc)->env == NULL) \
         return SQL_INVALID_HANDLE
#define NULL_CHECK_ENV_ACCESS_DESC(h) \
    if (h == NULL \
         || ((DESC*) h)->dbc == NULL \
         || ((DBC*) ((DESC*) h)->dbc)->env == NULL) \
         return SQL_INVALID_HANDLE

#endif // DRIVER_H
