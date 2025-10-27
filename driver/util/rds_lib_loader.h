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

#include <shared_mutex>
#include <unordered_map>

#include "rds_strings.h"

#include "../odbcapi.h"

#ifdef _WIN32
    #include <windows.h>
    #define MODULE_HANDLE HINSTANCE
    #define FUNC_HANDLE FARPROC
    #define RDS_LOAD_MODULE_DEFAULTS(module_name) RDS_LOAD_MODULE(module_name, LOAD_WITH_ALTERED_SEARCH_PATH)
    #ifdef UNICODE
inline HMODULE RDS_LOAD_MODULE(std::string module_name, DWORD load_flag) {
    std::vector<unsigned short> mod_name_vec = ConvertUTF8ToUTF16(module_name);
    unsigned short* mod_name_ushort = mod_name_vec.data();
    return LoadLibraryEx((SQLWCHAR*)mod_name_ushort, NULL, load_flag);
}
    #else
        #define RDS_LOAD_MODULE(module_name, load_flag) LoadLibraryEx(module_name.c_str(), NULL, load_flag)
    #endif // UNICODE
    #define RDS_FREE_MODULE(handle) FreeLibrary(handle)
    #define RDS_GET_FUNC(handle, fn_name) GetProcAddress(handle, fn_name)
#else // Unix (Linux / MacOS)
    #include <dlfcn.h>
    #define MODULE_HANDLE void*
    #define FUNC_HANDLE void*
    #define RDS_LOAD_MODULE_DEFAULTS(module_name) RDS_LOAD_MODULE(module_name.c_str(), RTLD_LAZY | RTLD_LOCAL)
    #define RDS_LOAD_MODULE(module_name, load_flag) dlopen(module_name, load_flag)
    #define RDS_FREE_MODULE(handle) dlclose(handle)
    #define RDS_GET_FUNC(handle, fn_name) dlsym(handle, fn_name)
#endif

struct RdsLibResult {
    bool fn_load_success;
    SQLRETURN fn_result;
    RDS_STR fn_name;
}; // RdsLibResult

class RdsLibLoader {
public:
    RdsLibLoader() = default;
    RdsLibLoader(RDS_STR library_path);
    ~RdsLibLoader();

    template<typename RDS_Func, typename... Args>
    RdsLibResult CallFunction(const RDS_STR& func_name, Args... args);
    virtual FUNC_HANDLE GetFunction(const RDS_STR& function_name);
    RDS_STR GetDriverPath();

protected:
private:
    RDS_STR driver_path;

    std::shared_mutex cache_lock;
    MODULE_HANDLE driver_handle;
    std::unordered_map<RDS_STR, FUNC_HANDLE> function_cache;
};

template <typename RDS_Func, typename... Args>
RdsLibResult RdsLibLoader::CallFunction(const RDS_STR& func_name, Args... args)
{
    FUNC_HANDLE driver_function = nullptr;
    // Try retrieving from cache
    {
        std::shared_lock lock(cache_lock);
        if (function_cache.contains(func_name)) {
            try {
                driver_function = function_cache.at(func_name);
            } catch (const std::out_of_range&) {
                // Should not happen but done to satisfy clang-tidy
                driver_function = nullptr;
            }
        }
    }
    // Cache miss
    if (!driver_function) {
        driver_function = GetFunction(func_name);
    }

    // Verify before function call
    SQLRETURN fn_ret = SQL_ERROR;
    bool fn_load = false;
    if (driver_function) {
        fn_load = true;
        RDS_Func rds_func = reinterpret_cast<RDS_Func>(const_cast<FUNC_HANDLE>(driver_function));
        fn_ret = (*rds_func)(args...);
    }

    return {
        .fn_load_success = fn_load,
        .fn_result = fn_ret,
        .fn_name = func_name
    };
}

#endif // RDS_LIB_LOADER_H
