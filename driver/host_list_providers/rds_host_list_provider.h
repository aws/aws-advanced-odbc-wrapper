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

#ifndef RDS_HOST_LIST_PROVIDER_H_
#define RDS_HOST_LIST_PROVIDER_H_

#include <string>
#include <vector>
#include <mutex>

#include "host_list_provider.h"

#include "cluster_topology_monitor.h"

#include "../host_info.h"

#include "../util/sliding_cache_map.h"
#include "../util/plugin_service.h"

class RdsHostListProvider : public HostListProvider {
public:
    RdsHostListProvider(std::shared_ptr<TopologyUtil> topology_util, PluginService* plugin_service);
    ~RdsHostListProvider();
    virtual std::vector<HostInfo> GetCurrentTopology(SQLHDBC hdbc, const HostInfo& initial_host);
    virtual std::vector<HostInfo> Refresh() override;
    virtual std::vector<HostInfo> ForceRefresh(bool verify_writer, uint32_t timeout_ms) override;
    virtual HOST_ROLE GetConnectionRole(SQLHDBC hdbc) override;
    virtual HostInfo GetConnectionInfo(SQLHDBC hdbc) override;
    virtual std::string GetClusterId() override;

private:
    std::shared_ptr<ClusterTopologyMonitor> GetOrCreateMonitor();

    std::shared_ptr<ClusterTopologyMonitor> monitor_;
    std::shared_ptr<TopologyUtil> topology_util_;
    PluginService* plugin_service_;
    HostInfo initial_host_info_;
    HostInfo template_host_info_;

    const uint32_t DEFAULT_TOPOLOGY_WAIT_MS = std::chrono::milliseconds(5000).count();

    // Shared resources
    static inline std::mutex monitor_map_mutex_;
    static inline std::unordered_map<std::string, std::pair<unsigned int, std::shared_ptr<ClusterTopologyMonitor>>> monitor_map_;
};

#endif // RDS_HOST_LIST_PROVIDER_H_
