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

#include "rds_lib_loader.h"

#include <mutex>

#include "logger_wrapper.h"

namespace {
std::string GetLastModuleLoadError()
{
#ifdef _WIN32
    const DWORD err_code = GetLastError();
    char err_buffer[512] = {0};
    const DWORD err_len = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err_code, 0,
        err_buffer, sizeof(err_buffer), nullptr);
    std::string err_msg = err_len > 0 ? std::string(err_buffer, err_len) : "";
    while (!err_msg.empty() && (err_msg.back() == '\n' || err_msg.back() == '\r')) {
        err_msg.pop_back();
    }
    return "error " + std::to_string(err_code) + (err_msg.empty() ? "" : ": " + err_msg);
#else
    const char* err_msg = dlerror();
    return err_msg ? std::string(err_msg) : "unknown dlopen error";
#endif
}
} // namespace

RdsLibLoader::RdsLibLoader(std::string library_path)
{
    driver_path = std::move(library_path);
    driver_handle = RDS_LOAD_MODULE_DEFAULTS(driver_path);
    if (!driver_handle) {
        load_error = GetLastModuleLoadError();
        LOG(ERROR) << "Failed to load underlying driver module [" << driver_path << "]: " << load_error;
    }
}

RdsLibLoader::~RdsLibLoader()
{
    /*
        Not calling RDS_FREE_MODULE(..),
        let OS cleanup on process termination to prevent incorrect unloading order of loaded library's dependencies
    */
    driver_handle = nullptr;
    if (function_cache) {
        function_cache->Clear();
        function_cache = nullptr;
    }
}

std::string RdsLibLoader::GetDriverPath()
{
    return driver_path;
}

bool RdsLibLoader::IsLoaded() const
{
    return driver_handle != nullptr;
}

std::string RdsLibLoader::GetLoadError()
{
    return load_error;
}

FUNC_HANDLE RdsLibLoader::GetFunction(const std::string &func_name)
{
    // Never look up symbols without a loaded module
    // it may pull Driver Manager's function pointer and cause deadlock
    if (!driver_handle) {
        return nullptr;
    }
    const FUNC_HANDLE driver_function = RDS_GET_FUNC(driver_handle, func_name.c_str());
    if (driver_function) {
        function_cache->InsertOrAssign(func_name, const_cast<FUNC_HANDLE>(driver_function));
    }
    return const_cast<FUNC_HANDLE>(driver_function);
}
