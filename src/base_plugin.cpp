#include "../include/base_plugin.h"
#include "../include/odbcapi.h"
#include "../include/connection_string_helper.h"

BasePlugin::BasePlugin() :
    plugin_name("Base")
{
}

BasePlugin::BasePlugin(DBC *dbc) :
    dbc(dbc),
    plugin_name("Base")
{
}

BasePlugin::BasePlugin(DBC *dbc, BasePlugin *next_plugin) :
    dbc(dbc),
    next_plugin(next_plugin),
    plugin_name("Base")
{
}

BasePlugin::~BasePlugin()
{
}

SQLRETURN BasePlugin::Connect(
    SQLHWND        WindowHandle,
    SQLCHAR *      OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    SQLRETURN ret = SQL_ERROR;
    bool has_conn_attr_errors = false;
    ENV* env = dbc->env;
    RDS_SQLAllocHandle alloc_proc = (RDS_SQLAllocHandle) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLAllocHandle");
    RDS_SQLDriverConnect drv_conn_proc = (RDS_SQLDriverConnect) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLDriverConnect");
    RDS_SQLSetConnectAttr set_connect_attr_proc = (RDS_SQLSetConnectAttr) RDS_GET_FUNC(env->wrapped_driver_handle, "SQLSetConnectAttr");

    // TODO - Should a new connect use a new underlying DBC?
    // Create Wrapped DBC if not already allocated
    if (!dbc->wrapped_dbc) {
        (*alloc_proc)(SQL_HANDLE_DBC, env->wrapped_env, &dbc->wrapped_dbc);
    }

    // DSN should be read from the original input
    // and a new connection string should be built without DSN & Driver
    RDS_STR conn_in = ConnectionStringHelper::BuildConnectionString(dbc->conn_attr);
    ret = (*drv_conn_proc)(dbc->wrapped_dbc, WindowHandle, AS_SQLTCHAR(conn_in.c_str()), SQL_NTS, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

    // Apply Tracked Connection Attributes
    for (auto const& [key, val] : dbc->attr_map) {
        has_conn_attr_errors |= (*set_connect_attr_proc)(dbc->wrapped_dbc, key, val.first, val.second);
    }

    // TODO - Error Handling for EnvAttr, ConnAttr, IsConnected
    // Successful Connection, but bad environment and/or connection attribute setting
    if (SQL_SUCCEEDED(ret) && has_conn_attr_errors) {
        // TODO - Set Error
        ret = SQL_SUCCESS_WITH_INFO;
    }
    return ret;
}
