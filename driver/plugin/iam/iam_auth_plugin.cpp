#include "iam_auth_plugin.h"

#include "../../util/aws_sdk_helper.h"
#include "../../util/connection_string_keys.h"

IamAuthPlugin::IamAuthPlugin(DBC *dbc) : IamAuthPlugin(dbc, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, BasePlugin *next_plugin) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "IAM";
    AwsSdkHelper::Init();

    // TODO - Helper to parse from URL
    std::string region = dbc->conn_attr.find(KEY_REGION) != dbc->conn_attr.end() ?
        AS_NARROW_STR(dbc->conn_attr.at(KEY_REGION)) : Aws::Region::US_EAST_1;
    auth_provider = std::make_shared<AuthProvider>(region);
}

IamAuthPlugin::~IamAuthPlugin()
{
    AwsSdkHelper::Shutdown();
    auth_provider.reset();
}

SQLRETURN IamAuthPlugin::Connect(
    SQLHWND        WindowHandle,
    SQLCHAR *      OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    auto map_end_itr = dbc->conn_attr.end();
    std::string server = dbc->conn_attr.find(KEY_SERVER) != map_end_itr ?
        AS_NARROW_STR(dbc->conn_attr.at(KEY_SERVER)) : "";
    // TODO - Helper to parse from URL
    std::string region = dbc->conn_attr.find(KEY_REGION) != map_end_itr ?
        AS_NARROW_STR(dbc->conn_attr.at(KEY_REGION)) : "";
    std::string port = dbc->conn_attr.find(KEY_PORT) != map_end_itr ?
        AS_NARROW_STR(dbc->conn_attr.at(KEY_PORT)) : "";
    std::string username = dbc->conn_attr.find(KEY_DB_USERNAME) != map_end_itr ?
        AS_NARROW_STR(dbc->conn_attr.at(KEY_DB_USERNAME)) : "";

    // TODO - Proper error handling for missing parameters
    if (server.empty() || region.empty() || port.empty() || username.empty()) {
        if (dbc->err) delete dbc->err;
        dbc->err = new ERR_INFO("Missing required parameters for IAM Authentication", 12345, "12345");
        return SQL_ERROR;
    }

    // TODO - Custom expiration time
    std::pair<std::string, bool> token = auth_provider->GetToken(server, region, port, username, true);

    SQLRETURN ret = SQL_ERROR;
    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, AS_RDS_STR(token.first.c_str()));
    ret = next_plugin->Connect(WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

    // Unsuccessful connection using cached token
    //  Skip cache and generate a new token to retry
    if (!SQL_SUCCEEDED(ret) && token.second) {
        token = auth_provider->GetToken(server, region, port, username, false);
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, AS_RDS_STR(token.first.c_str()));
        ret = next_plugin->Connect(WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    return ret;
}
