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

#ifndef TOPOLOGY_SERVICE_H_
#define TOPOLOGY_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../host_info.h"
#include "sliding_cache_map.h"

static inline std::shared_ptr<SlidingCacheMap<std::string, std::vector<HostInfo>>> topology_map_
    = std::make_shared<SlidingCacheMap<std::string, std::vector<HostInfo>>>();

struct HostFilter {
    std::set<std::string> allowed_host_ids;
    std::set<std::string> blocked_host_ids;

    bool operator==(const HostFilter& other) const
    {
        return allowed_host_ids == other.allowed_host_ids
            && blocked_host_ids == other.blocked_host_ids;
    }
};

class TopologyService {
public:
    TopologyService() = default;
    ~TopologyService() = default;
    TopologyService(std::string cluster_id);
    virtual std::vector<HostInfo> GetHosts();
    virtual void SetHosts(const std::vector<HostInfo>& hosts);
    virtual std::vector<HostInfo> GetFilteredHosts();
    virtual void SetHostFilter(HostFilter hosts);

    static std::string InitClusterId(std::map<std::string, std::string>& conn_info);

protected:
private:
    std::string cluster_id;
    HostFilter host_filter = {};
};

#endif // TOPOLOGY_SERVICE_H_
