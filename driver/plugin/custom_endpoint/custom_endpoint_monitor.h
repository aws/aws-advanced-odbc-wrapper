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

#ifndef CUSTOM_ENDPOINT_MONITOR_H_
#define CUSTOM_ENDPOINT_MONITOR_H_

#include "../base_plugin.h"
#include "../../driver.h"

#include "../../util/topology_service.h"
#include "../../util/sliding_cache_map.h"

#include <thread>
#include <memory>

class CustomEndpointMonitor {
public:
    CustomEndpointMonitor(
        const std::shared_ptr<TopologyService>& topology_service,
        const std::string& endpoint,
        std::string region,
        int64_t refresh_rate_ms,
        int64_t max_refresh_rate_ms,
        int exponential_backoff_rate);
    ~CustomEndpointMonitor();

    virtual void Run();
    virtual bool HasInfo();

private:
    void IncreaseDelay();
    void DecreaseDelay();

    static SlidingCacheMap<std::string, HostFilter> endpoint_cache;

    std::shared_ptr<std::thread> monitoring_thread_;
    std::atomic<bool> is_running_;

    std::shared_ptr<TopologyService> topology_service_;
    std::string endpoint_;
    std::string endpoint_identifier_;
    std::string region_;

    std::chrono::milliseconds refresh_rate_ms_;
    std::chrono::milliseconds min_refresh_rate_ms_;
    std::chrono::milliseconds max_refresh_rate_ms_;
    int exponential_backoff_rate_;

    const std::chrono::minutes UNAUTHORIZED_SLEEP_DIR = std::chrono::minutes(5);
};

#endif // CUSTOM_ENDPOINT_MONITOR_H_
