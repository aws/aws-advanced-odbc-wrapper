#ifndef IAM_AUTH_PLUGIN_H_
#define IAM_AUTH_PLUGIN_H_

#include "../base_plugin.h"

class IamAuthPlugin : public BasePlugin {
public:
    IamAuthPlugin();
    IamAuthPlugin(DBC* dbc);
    IamAuthPlugin(DBC* dbc, BasePlugin* next_plugin);
    ~IamAuthPlugin() override;

    SQLRETURN Connect(
        SQLHWND        WindowHandle,
        SQLCHAR *      OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;
};

#endif // IAM_AUTH_PLUGIN_H_
