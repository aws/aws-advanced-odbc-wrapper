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

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

class MonitorService;
class StorageService;

// Process-wide singleton owning the services shared by all connections.
//
// Like LoggerWrapper, its lifetime follows the allocated SQLHENVs rather than DLL load / unload hooks:
// RDS_AllocEnv calls Acquire() and RDS_FreeEnv calls Release(). The instance and its services are
// created when the count goes from 0 to 1, and destroyed when it drops back to 0.
//
// TODO - Move LoggerWrapper in here, logging is only initialized once and its level / location never change
class CoreService {
public:
    // Both run under LifecycleMutex(), so the core services must not allocate or free an SQLHENV
    // from their constructors / destructors, it would deadlock
    static void Acquire();
    static void Release();

    // nullptr while no SQLHENV is allocated. Only valid while the caller keeps an SQLHENV allocated,
    // directly or through one of its DBC / STMT / DESC handles.
    [[nodiscard]] static CoreService* GetInstance();

    [[nodiscard]] StorageService* GetStorageService() const;
    [[nodiscard]] MonitorService* GetMonitorService() const;

    // Prevent copy constructors
    CoreService(const CoreService&) = delete;
    CoreService(CoreService&&) = delete;
    CoreService& operator=(const CoreService&) = delete;
    CoreService& operator=(CoreService&&) = delete;

private:
    // Only created and destroyed by Acquire() / Release()
    CoreService();
    ~CoreService();

    // Never destroyed, so Acquire() / Release() keep working when reached from static destructors at exit
    static std::mutex& LifecycleMutex();

    // Members are destroyed in reverse order, so monitors go before the storage they may use
    std::unique_ptr<StorageService> storage_service_;
    std::unique_ptr<MonitorService> monitor_service_;

    // Number of allocated SQLHENVs, guarded by LifecycleMutex()
    static uint32_t env_count_;
    // Set and cleared under LifecycleMutex(), read lock-free by GetInstance()
    static std::atomic<CoreService*> instance_;
};

#endif // CORE_SERVICE_H_
