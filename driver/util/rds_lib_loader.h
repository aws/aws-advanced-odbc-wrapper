#ifndef RDS_LIB_LOADER_H
#define RDS_LIB_LOADER_H

#ifdef _WIN32
    #include <windows.h>
    #define MODULE_HANDLE HINSTANCE
    #define FUNC_HANDLE FARPROC
    #define RDS_LOAD_MODULE_DEFAULTS(module_name) RDS_LOAD_MODULE(module_name, LOAD_WITH_ALTERED_SEARCH_PATH)
    #define RDS_LOAD_MODULE(module_name, load_flag) LoadLibraryEx(module_name, NULL, load_flag)
    #define RDS_FREE_MODULE(handle) FreeLibrary(handle)
    #define RDS_GET_FUNC(handle, fn_name) GetProcAddress(handle, fn_name)
#else // Unix (Linux / MacOS)
    #include <dlfcn.h>
    #define MODULE_HANDLE void*
    #define FUNC_HANDLE void*
    #define RDS_LOAD_MODULE_DEFAULTS(module_name) RDS_LOAD_MODULE(module_name, RTLD_LAZY | RTLD_LOCAL)
    #define RDS_LOAD_MODULE(module_name, load_flag) dlopen(module_name, load_flag)
    #define RDS_FREE_MODULE(handle) dlclose(handle)
    #define RDS_GET_FUNC(handle, fn_name) dlsym(handle, fn_name)
#endif

#include <shared_mutex>
#include <unordered_map>

#include "rds_strings.h"

#include "../odbcapi.h"

struct RdsLibResult {
    bool fn_load_success;
    SQLRETURN fn_result;
}; // RdsLibResult

class RdsLibLoader {
public:
    RdsLibLoader(RDS_STR library_path);
    ~RdsLibLoader();

    template<typename RDS_Func, typename... Args>
    RdsLibResult CallFunction(RDS_STR func_name, Args... args);

    RDS_STR GetDriverPath();

protected:
private:
    RDS_STR driver_path;

    FUNC_HANDLE GetFunction(RDS_STR function_name);
    std::shared_mutex cache_lock;
    MODULE_HANDLE driver_handle;
    std::unordered_map<RDS_STR, FUNC_HANDLE> function_cache;
};

template <typename RDS_Func, typename... Args>
inline RdsLibResult RdsLibLoader::CallFunction(RDS_STR func_name, Args... args)
{
    FUNC_HANDLE driver_function = nullptr;
    // Try retrieving from cache
    {
        std::shared_lock lock(cache_lock);
        if (function_cache.find(func_name) != function_cache.end()) {
            driver_function = function_cache.at(func_name);
        }
    }
    // Cache miss
    if (!driver_function) {
        driver_function = GetFunction(func_name);
    }

    // Verify before function call
    SQLRETURN fn_ret = SQL_ERROR;
    bool fn_load_success = false;
    if (driver_function) {
        fn_load_success = true;
        RDS_Func rds_func = reinterpret_cast<RDS_Func>(driver_function);
        fn_ret = (*rds_func)(args...);
    }

    return RdsLibResult{fn_load_success, fn_ret};
}

#endif // RDS_LIB_LOADER_H
