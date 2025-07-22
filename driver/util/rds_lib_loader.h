// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

    FUNC_HANDLE GetFunction(RDS_STR function_name);

    RDS_STR GetDriverPath();

protected:
private:
    RDS_STR driver_path;

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

#define NULL_CHECK_CALL_LIB_FUNC(lib_loader, fn_type, fn_name, ...) lib_loader ? \
    lib_loader->CallFunction<fn_type>(fn_name, __VA_ARGS__) \
    : RdsLibResult{.fn_load_success = false, .fn_result = SQL_ERROR}

#endif // RDS_LIB_LOADER_H
