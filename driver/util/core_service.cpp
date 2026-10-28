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

#include "core_service.h"

#include <array>
#include <cstddef>
#include <new>

#include "logger_wrapper.h"

CoreService::CoreService()
{
    LOG(INFO) << "Created Core Service";
}

CoreService& CoreService::Instance()
{
    // Not "static CoreService instance;", the compiler would destroy that at exit or DLL unload, while code running
    // later, e.g. an app's own static destructors, can still free an ENV. Here the compiler only owns raw memory,
    // which has nothing to destroy, and the instance built in it is never destroyed.
    //
    // Static storage rather than the heap, so nothing is left behind each time the Driver Manager unloads the DLL.
    // Aligned for CoreService, as a byte array alone may start at any address.
    alignas(CoreService) static std::array<std::byte, sizeof(CoreService)> storage;
    // Placement new, building the instance inside storage. As a function local static it runs once, on the first
    // call, and is thread safe. If the constructor throws, the next call tries again
    static CoreService* const INSTANCE = new (storage.data()) CoreService();
    return *INSTANCE;
}

StorageService& CoreService::GetStorageService()
{
    return storage_service_;
}

MonitorService& CoreService::GetMonitorService()
{
    return monitor_service_;
}

uint32_t CoreService::GetEnvCount()
{
    const std::lock_guard<std::mutex> lock(env_count_mutex_);
    return env_count_;
}

void CoreService::OnEnvAllocated()
{
    const std::lock_guard<std::mutex> lock(env_count_mutex_);
    ++env_count_;
}

void CoreService::OnEnvFreed()
{
    const std::lock_guard<std::mutex> lock(env_count_mutex_);
    if (env_count_ == 0) {
        // Each counted ENV is freed once, so this is a bug in the driver
        LOG(ERROR) << "More environment handles freed than allocated, ignoring";
        return;
    }
    if (--env_count_ == 0) {
        // Under the lock, so an ENV allocated meanwhile waits until the monitors are released
        LOG(INFO) << "Last environment handle freed, releasing monitors";
        monitor_service_.ReleaseResources();
    }
}
