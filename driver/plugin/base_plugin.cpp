#include "base_plugin.h"

#include "../odbcapi.h"
#include "../util/connection_string_helper.h"
#include "../util/rds_lib_loader.h"

BasePlugin::BasePlugin(DBC *dbc) : BasePlugin(dbc, nullptr) {}

BasePlugin::BasePlugin(DBC *dbc, BasePlugin *next_plugin) :
    dbc(dbc),
    next_plugin(next_plugin),
    plugin_name("BasePlugin")
{
    if (!dbc) {
        throw std::runtime_error("DBC cannot be null.");
    }
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

    // TODO - Should a new connect use a new underlying DBC?
    // Create Wrapped DBC if not already allocated
    RdsLibResult res;
    if (!dbc->wrapped_dbc) {
        res = env->driver_lib_loader->CallFunction<RDS_SQLAllocHandle>(AS_RDS_STR("SQLAllocHandle"),
            SQL_HANDLE_DBC, env->wrapped_env, &dbc->wrapped_dbc
        );
    }

    // DSN should be read from the original input
    // and a new connection string should be built without DSN & Driver
    RDS_STR conn_in = ConnectionStringHelper::BuildConnectionString(dbc->conn_attr);
    res = env->driver_lib_loader->CallFunction<RDS_SQLDriverConnect>(AS_RDS_STR("SQLDriverConnect"),
        dbc->wrapped_dbc, WindowHandle, AS_SQLTCHAR(conn_in.c_str()), SQL_NTS, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion
    );

    if (res.fn_load_success) {
        ret = res.fn_result;
    }

    // Apply Tracked Connection Attributes
    for (auto const& [key, val] : dbc->attr_map) {
        res = env->driver_lib_loader->CallFunction<RDS_SQLSetConnectAttr>(AS_RDS_STR("SQLSetConnectAttr"),
            dbc->wrapped_dbc, key, val.first, val.second
        );
        has_conn_attr_errors != res.fn_result;
    }

    // TODO - Error Handling for ConnAttr, IsConnected
    // Successful Connection, but bad environment and/or connection attribute setting
    if (SQL_SUCCEEDED(ret) && has_conn_attr_errors) {
        // TODO - Set Error
        ret = SQL_SUCCESS_WITH_INFO;
    }
    return ret;
}
