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

#ifndef BASE_ROUTING_H_
#define BASE_ROUTING_H_

#include "../blue_green_phase.h"
#include "../blue_green_role.h"
#include "../blue_green_status.h"

#include "../../../util/sliding_cache_map.h"

#include <chrono>
#include <string>

class BaseRouting {
public:
    BaseRouting(std::string host_port, BlueGreenRole role);
    virtual void Delay(
        std::chrono::milliseconds delay_ms,
        BlueGreenStatus status,
        const std::shared_ptr<SlidingCacheMap<std::string, BlueGreenStatus>> status_cache,
        std::string id);
    virtual std::chrono::system_clock::time_point GetCurrTime() const;

    std::string ToString() const;
    bool operator==(const BaseRouting& other) const {
        return route_class_ == other.route_class_ && host_port_ == other.host_port_ && role_ == role_;
    };

protected:
    std::string route_class_;
    std::string host_port_;
    BlueGreenRole role_;
    static inline const std::chrono::milliseconds DEFAULT_CONNECT_TIMEOUT_MS = std::chrono::seconds(30);
    static inline const std::chrono::milliseconds SLEEP_DURATION_MS = std::chrono::milliseconds(100);
    static inline const std::chrono::milliseconds MIN_SLEEP_MS = std::chrono::milliseconds(50);
};

#endif // BASE_ROUTING_H_
