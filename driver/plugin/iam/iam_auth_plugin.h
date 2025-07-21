#ifndef IAM_AUTH_PLUGIN_H_
#define IAM_AUTH_PLUGIN_H_

#include "../../util/auth_provider.h"

#include "../base_plugin.h"

class IamAuthPlugin : public BasePlugin {
public:
    IamAuthPlugin();
    IamAuthPlugin(DBC* dbc);
    IamAuthPlugin(DBC* dbc, BasePlugin* next_plugin);
    ~IamAuthPlugin() override;

    SQLRETURN Connect(
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

private:
    std::shared_ptr<AuthProvider> auth_provider;
};

#endif // IAM_AUTH_PLUGIN_H_
