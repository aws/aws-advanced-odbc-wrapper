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

#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "concurrent_map.h"
#include "rds_strings.h"

#include "../odbcapi.h"

#include "windows_headers.h"

#ifndef _WIN32 // Unix (Linux / MacOS)
    #include <dlfcn.h>
#endif

namespace RdsPlatform {
#ifdef _WIN32
using ModuleHandle = HMODULE;
using FuncHandle = FARPROC;
#else
using ModuleHandle = void*;
using FuncHandle = void*;
#endif

inline ModuleHandle OpenLibrary(const std::string& library_path)
{
#ifdef _WIN32
    #ifdef UNICODE
    std::vector<uint16_t> path_utf16 = ConvertUTF8ToUTF16(library_path);
    return LoadLibraryEx(reinterpret_cast<SQLWCHAR*>(path_utf16.data()), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    #else
    return LoadLibraryEx(library_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    #endif // UNICODE
#else
    return dlopen(library_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
}

inline bool CloseLibrary(ModuleHandle handle)
{
#ifdef _WIN32
    return FreeLibrary(handle) != 0;
#else
    return dlclose(handle) == 0;
#endif
}

inline FuncHandle FindSymbol(ModuleHandle handle, const char* symbol_name)
{
#ifdef _WIN32
    return GetProcAddress(handle, symbol_name);
#else
    return dlsym(handle, symbol_name);
#endif
}
} // namespace RdsPlatform

struct RdsLibResult {
    bool fn_load_success;
    SQLRETURN fn_result;
    std::string fn_name;
}; // RdsLibResult

class RdsLibLoader {
public:
    RdsLibLoader() = default;
    explicit RdsLibLoader(std::string library_path);
    ~RdsLibLoader();

    template<typename RdsFunc, typename... Args>
    RdsLibResult CallFunction(std::string_view func_name, Args... args);

    template<typename RdsFunc, typename... Args>
    static RdsLibResult CallFunctionChecked(
        const std::shared_ptr<RdsLibLoader>& lib_loader, std::string_view func_name, Args... args);

    virtual RdsPlatform::FuncHandle GetFunction(const std::string& function_name);
    std::string GetDriverPath();
    [[nodiscard]] bool IsLoaded() const;
    std::string GetLoadError();

protected:
private:
    std::string driver_path_;
    std::string load_error_;

    RdsPlatform::ModuleHandle driver_handle_ = nullptr;

    std::shared_ptr<ConcurrentMap<std::string, RdsPlatform::FuncHandle>> function_cache_ =
        std::make_shared<ConcurrentMap<std::string, RdsPlatform::FuncHandle>>();
};

template <typename RdsFunc, typename... Args>
RdsLibResult RdsLibLoader::CallFunction(std::string_view func_name, Args... args)
{
    static_assert(std::is_invocable_r_v<SQLRETURN, RdsFunc, Args...>,
        "arguments do not match the signature of the ODBC function named by RdsFunc");

    const std::string func_key(func_name);
    // Try retrieving from cache
    RdsPlatform::FuncHandle driver_function = function_cache_->Get(func_key);
    // Cache miss
    if (!driver_function) {
        driver_function = GetFunction(func_key);
    }

    // Verify before function call
    SQLRETURN fn_ret = SQL_ERROR;
    bool fn_load = false;
    if (driver_function) {
        fn_load = true;
        const RdsFunc rds_func = reinterpret_cast<RdsFunc>(driver_function);
        fn_ret = (*rds_func)(args...);
    }

    return {
        .fn_load_success = fn_load,
        .fn_result = fn_ret,
        .fn_name = func_key,
    };
}

template <typename RdsFunc, typename... Args>
RdsLibResult RdsLibLoader::CallFunctionChecked(
    const std::shared_ptr<RdsLibLoader>& lib_loader, std::string_view func_name, Args... args)
{
    if (!lib_loader) {
        return {
            .fn_load_success = false,
            .fn_result = SQL_ERROR,
            .fn_name = std::string(func_name),
        };
    }
    return lib_loader->CallFunction<RdsFunc>(func_name, args...);
}

#endif // RDS_LIB_LOADER_H
