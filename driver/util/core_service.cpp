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

#include "logger_wrapper.h"
#include "monitoring/monitor_service.h"
#include "storage/storage_service.h"

uint32_t CoreService::env_count_ = 0;
std::atomic<CoreService*> CoreService::instance_{nullptr};

CoreService::CoreService()
    : storage_service_{ std::make_unique<StorageService>() },
      monitor_service_{ std::make_unique<MonitorService>() } {}

CoreService::~CoreService() = default;

std::mutex& CoreService::LifecycleMutex()
{
    // Deliberately never destroyed. If an app exits without freeing its handles, static destructors in
    // other files (e.g. a monitor registry) can still reach RDS_FreeEnv after this file's statics are gone
    static auto* const MUTEX = new std::mutex();
    return *MUTEX;
}

void CoreService::Acquire()
{
    const std::lock_guard<std::mutex> lock(LifecycleMutex());
    if (env_count_ == 0) {
        // Count only moves after a successful construction, so a throw leaves the state unchanged
        instance_.store(new CoreService());
        LOG(INFO) << "Created Core Service";
    }
    ++env_count_;
}

void CoreService::Release()
{
    const std::lock_guard<std::mutex> lock(LifecycleMutex());
    if (env_count_ == 0) {
        LOG(WARNING) << "Core Service released more times than acquired, ignoring";
        return;
    }
    // Only the transitions log. Releases can run from static destructors at exit, where logging is not
    // safe, but the count never reaches 0 there as the app's own SQLHENV is still allocated
    if (--env_count_ == 0) {
        // Unpublish first so GetInstance() never hands out an instance being destroyed
        delete instance_.exchange(nullptr);
        LOG(INFO) << "Shut down Core Service";
    }
}

CoreService* CoreService::GetInstance()
{
    return instance_.load();
}

StorageService* CoreService::GetStorageService() const
{
    return storage_service_.get();
}

MonitorService* CoreService::GetMonitorService() const
{
    return monitor_service_.get();
}
