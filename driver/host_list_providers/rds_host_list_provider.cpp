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

#include "rds_host_list_provider.h"

#include "../util/logger_wrapper.h"

RdsHostListProvider::RdsHostListProvider(std::shared_ptr<TopologyUtil> topology_util, PluginService* plugin_service) :
    topology_util_{ std::move(topology_util) },
    plugin_service_{ plugin_service },
    HostListProvider(plugin_service->GetClusterId())
{
    this->monitor_ = GetOrCreateMonitor();
}

RdsHostListProvider::~RdsHostListProvider() {
    const std::lock_guard<std::mutex> lock_guard(monitor_map_mutex_);
    if (auto itr = monitor_map_.find(this->cluster_id_); itr != monitor_map_.end()) {
        std::pair<unsigned int, std::shared_ptr<ClusterTopologyMonitor>>& pair = itr->second;
        if (pair.first == 1) {
            monitor_map_.erase(this->cluster_id_);
            LOG(INFO) << "Shut down Cluster Topology Monitor count for: " << this->cluster_id_;
        } else {
            pair.first--;
            LOG(INFO) << "Decremented Cluster Topology Monitor count for: " << this->cluster_id_ << ", to: " << pair.first;
        }
    }
}

std::vector<HostInfo> RdsHostListProvider::GetCurrentTopology(SQLHDBC hdbc, const HostInfo& initial_host) {
    return this->topology_util_->QueryTopology(hdbc, initial_host, this->template_host_info_);
}

std::vector<HostInfo> RdsHostListProvider::Refresh() {
    std::vector<HostInfo> hosts = this->plugin_service_->GetHosts();
    if (hosts.empty()) {
        hosts = this->ForceRefresh(false, DEFAULT_TOPOLOGY_WAIT_MS);
    }
    return hosts;
}

std::vector<HostInfo> RdsHostListProvider::ForceRefresh(bool verify_writer, uint32_t timeout_ms) {
    return this->monitor_->ForceRefresh(verify_writer, timeout_ms);
}

HOST_ROLE RdsHostListProvider::GetConnectionRole(SQLHDBC hdbc) {
    return this->topology_util_->GetConnectionRole(hdbc);
}

HostInfo RdsHostListProvider::GetConnectionInfo(SQLHDBC hdbc) {
    const std::string instance_id = this->topology_util_->GetInstanceId(hdbc);
    const std::vector<HostInfo> hosts = this->Refresh();
    HostInfo host;
    for (const HostInfo& hi : hosts) {
        if (instance_id == hi.GetHostId()) {
            host = hi;
            break;
        }
    }
    return host;
}

std::string RdsHostListProvider::GetClusterId() {
    return cluster_id_;
}

std::shared_ptr<ClusterTopologyMonitor> RdsHostListProvider::GetOrCreateMonitor() {
    const std::lock_guard<std::mutex> lock_guard(monitor_map_mutex_);
    std::shared_ptr<ClusterTopologyMonitor> monitor;
    if (auto itr = monitor_map_.find(this->cluster_id_); itr != monitor_map_.end()) {
        std::pair<unsigned int, std::shared_ptr<ClusterTopologyMonitor>>& pair = itr->second;
        pair.first++;
        monitor = pair.second;
        LOG(INFO) << "Incremented Cluster Topology Monitor count for: " << this->cluster_id_ << ", to: " << pair.first;
    } else {
        monitor = std::make_shared<ClusterTopologyMonitor>(
            this->plugin_service_, this->topology_util_
        );
        const std::pair pair = {1, monitor};
        if (!this->plugin_service_->GetOriginalConnAttr().contains(KEY_RDS_TEST_CONN)) {
            monitor->StartMonitor();
        }
        monitor_map_.insert_or_assign(this->cluster_id_, pair);
        LOG(INFO) << "Created Cluster Topology Monitor for: " << this->cluster_id_;
    }
    return monitor;
}
