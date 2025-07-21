#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/rds/RDSClient.h>
#include <aws/sts/STSClient.h>

#include "iam_auth_plugin.h"

#include "../../util/connection_string_keys.h"

static Aws::SDKOptions sdk_opts;

IamAuthPlugin::IamAuthPlugin() : BasePlugin()
{
    this->plugin_name = "IAM";
    // TODO - AWS API Helper
    //  init and shutdown shouldn't be called multiple times
    Aws::InitAPI(sdk_opts);
}

IamAuthPlugin::IamAuthPlugin(DBC *dbc) : BasePlugin(dbc)
{
    this->plugin_name = "IAM";
    Aws::InitAPI(sdk_opts);
}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, BasePlugin *next_plugin) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "IAM";
    Aws::InitAPI(sdk_opts);
}

IamAuthPlugin::~IamAuthPlugin()
{
    Aws::ShutdownAPI(sdk_opts);
}

SQLRETURN IamAuthPlugin::Connect(
    SQLHWND        WindowHandle,
    SQLCHAR *      OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    Aws::Auth::DefaultAWSCredentialsProviderChain cred_provider;
    Aws::Auth::AWSCredentials credentials = cred_provider.GetAWSCredentials();
    Aws::RDS::RDSClientConfiguration rds_client_cfg;
    Aws::RDS::RDSClient client(credentials, rds_client_cfg);

    auto map_end_itr = dbc->conn_attr.end();
    std::string database = dbc->conn_attr.find(KEY_DATABASE) != map_end_itr ?
        AS_NARROW_STR(dbc->conn_attr.at(KEY_DATABASE)) : "";
    // TODO - Helper to parse from URL
    std::string region = dbc->conn_attr.find(KEY_REGION) != map_end_itr ?
        AS_NARROW_STR(dbc->conn_attr.at(KEY_REGION)) : "";
    // TODO - Helper to parse / guess from URL
    std::string port = dbc->conn_attr.find(KEY_PORT) != map_end_itr ?
        AS_NARROW_STR(dbc->conn_attr.at(KEY_PORT)) : "";
    std::string username = dbc->conn_attr.find(KEY_DB_USERNAME) != map_end_itr ?
        AS_NARROW_STR(dbc->conn_attr.at(KEY_DB_USERNAME)) : "";
    
    // TODO - Error handling for missing parameters
    if (database.empty() || region.empty() || port.empty() || username.empty()) {
        dbc->err = new ERR_INFO("Missing required parameters for IAM Authentication", 12345, "12345");
        return SQL_ERROR;
    }

    Aws::String token = client.GenerateConnectAuthToken(database.c_str(), region.c_str(), std::atoi(port.c_str()), username.c_str());

    // TODO - Cache token & try/catch to refresh expired tokens
    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, AS_RDS_STR(token.c_str()));

    return next_plugin->Connect(WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
}
