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

#ifndef CORE_SERVICE_H_
#define CORE_SERVICE_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include "monitoring/monitor_service.h"
#include "storage/storage_service.h"

class RdsLibLoader;

// Owns the services shared by all connections. One instance per loaded driver library, so the ANSI and the
// Unicode drivers each have their own.
//
// Created by the first Instance() call and never destroyed. It lives in static storage, so nothing is left on the
// heap when the DLL is unloaded, and static destructors running late at exit can still reach it.
//
// Counts the ENVs allocated by the app or Driver Manager. The driver's own ENVs, e.g. a monitor's, are not counted,
// otherwise the count could never reach 0 while monitors run. The Driver Manager may unload the DLL right after
// the last ENV is freed, so the monitors are released then. The stored data is kept, and an ENV allocated
// afterwards reuses the same instance.
//
// Also keeps one ENV of each base driver open, see PinBaseDriver().
class CoreService {
public:
    [[nodiscard]] static CoreService& Instance();

    [[nodiscard]] StorageService& GetStorageService();
    [[nodiscard]] MonitorService& GetMonitorService();

    // Only called by RDS_AllocEnv / RDS_FreeEnv, for ENVs with counted_by_core_service set.
    // The last OnEnvFreed() releases the monitors, which must not allocate or free a counted ENV
    void OnEnvAllocated();
    void OnEnvFreed();
    [[nodiscard]] uint32_t GetEnvCount();

    // Allocates one ENV of the base driver, once per driver path, and never frees it. Some base drivers tear down
    // their global state when their last ENV is freed, then set it up again on the next connect, and that is not
    // thread safe. MySQL Connector/ODBC on Unix does this: an app connecting at the same time as one of its monitors
    // can fail with HY001 "Memory allocation error". The wrapper never unloads a base driver, so keeping it set up
    // only costs one ENV. Must be called before any monitor of the connection starts. Thread safe
    void PinBaseDriver(const std::shared_ptr<RdsLibLoader>& lib_loader);
    [[nodiscard]] bool IsBaseDriverPinned(const std::string& driver_path);

    // Never destroyed, see Instance()
    ~CoreService() = delete;

    // Prevent copy constructors
    CoreService(const CoreService&) = delete;
    CoreService(CoreService&&) = delete;
    CoreService& operator=(const CoreService&) = delete;
    CoreService& operator=(CoreService&&) = delete;

private:
    CoreService();

    std::mutex env_count_mutex_;
    // Counted ENVs, guarded by env_count_mutex_
    uint32_t env_count_ = 0;

    std::mutex pinned_drivers_mutex_;
    // Paths of the base drivers with a pinned ENV, guarded by pinned_drivers_mutex_
    std::unordered_set<std::string> pinned_driver_paths_;

    // Constructed in this order, as monitors may use the storage
    StorageService storage_service_;
    MonitorService monitor_service_;
};

#endif // CORE_SERVICE_H_
