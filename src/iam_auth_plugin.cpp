#include "../include/iam_auth_plugin.h"

#include "../include/connection_string_keys.h"

IamAuthPlugin::IamAuthPlugin() : BasePlugin()
{
    this->plugin_name = "IAM";
}

IamAuthPlugin::IamAuthPlugin(DBC *dbc) : BasePlugin(dbc)
{
    this->plugin_name = "IAM";
}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, BasePlugin *next_plugin) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "IAM";
}

IamAuthPlugin::~IamAuthPlugin()
{
}

SQLRETURN IamAuthPlugin::Connect(
    SQLHWND        WindowHandle,
    SQLCHAR *      OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    // Dummy implementation
    dbc->conn_attr.insert_or_assign(KEY_DB_USERNAME, "postgres");
    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, "password");

    return next_plugin->Connect(WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
}
