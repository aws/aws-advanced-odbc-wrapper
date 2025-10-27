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

RdsLibLoader::RdsLibLoader(RDS_STR library_path)
{
    driver_path = std::move(library_path);
    driver_handle = RDS_LOAD_MODULE_DEFAULTS(driver_path);
}

RdsLibLoader::~RdsLibLoader()
{
    if (driver_handle) {
        RDS_FREE_MODULE(driver_handle);
    }
}

RDS_STR RdsLibLoader::GetDriverPath()
{
    return driver_path;
}

FUNC_HANDLE RdsLibLoader::GetFunction(const RDS_STR &func_name)
{
    const std::string converted_function_name = func_name;
    const FUNC_HANDLE driver_function = RDS_GET_FUNC(driver_handle, converted_function_name.c_str());
    if (driver_function) {
        const std::unique_lock lock(cache_lock);
        function_cache.insert_or_assign(func_name, const_cast<FUNC_HANDLE>(driver_function));
    }
    return const_cast<FUNC_HANDLE>(driver_function);
}
