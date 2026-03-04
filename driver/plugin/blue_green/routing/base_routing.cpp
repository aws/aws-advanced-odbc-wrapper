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

#include "base_routing.h"

#include "../blue_green_status.h"

#include "../../../util/sliding_cache_map.h"

#include <chrono>
#include <format>
#include <thread>

BaseRouting::BaseRouting(std::string host_port, BlueGreenRole role) : host_port_{ host_port }, role_{ role }, route_class_{ "BaseRouting" } {}

void BaseRouting::Delay(
    std::chrono::milliseconds delay_ms,
    BlueGreenStatus status,
    std::shared_ptr<SlidingCacheMap<std::string, BlueGreenStatus>> status_cache,
    std::string id)
{
    std::chrono::system_clock::time_point start = GetCurrTime();
    std::chrono::system_clock::time_point end = start + delay_ms;
    std::chrono::milliseconds min_delay = delay_ms < MIN_SLEEP_MS ? delay_ms : MIN_SLEEP_MS;

    BlueGreenStatus cached_status = status_cache->Get(id);

    if (cached_status.GetCurrentPhase().GetPhase() == BlueGreenPhase::UNKNOWN) {
        std::this_thread::sleep_for(delay_ms);
    } else {
        do {
            std::this_thread::sleep_for(min_delay);
        } while ((cached_status = status_cache->Get(id)) == status && GetCurrTime() < end);
    }
}

std::chrono::system_clock::time_point BaseRouting::GetCurrTime() const {
    return std::chrono::system_clock::now();
}

std::string BaseRouting::ToString() const {
    return std::format("{}, {}", host_port_, role_.ToString());
}
