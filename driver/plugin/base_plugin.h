#ifndef BASE_PLUGIN_H_
#define BASE_PLUGIN_H_

#include "../driver.h"

struct DBC;

class BasePlugin {
public:
    BasePlugin();
    BasePlugin(DBC* dbc);
    BasePlugin(DBC* dbc, BasePlugin* next_plugin);
    virtual ~BasePlugin();

    virtual SQLRETURN Connect(
        SQLHWND        WindowHandle,
        SQLCHAR *      OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion);

protected:
    // TODO - Rethink this, DBC will have reference this, and this will reference the DBC
    DBC* dbc;
    BasePlugin* next_plugin;
    std::string plugin_name;

private:
};

#endif // BASE_PLUGIN_H_
