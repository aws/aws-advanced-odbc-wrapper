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

#include "../util/connection_string_keys.h"
#include "../util/rds_utils.h"
#include "../util/logger_wrapper.h"

RdsHostListProvider::RdsHostListProvider(const std::shared_ptr<TopologyUtil>& topology_util, const std::shared_ptr<PluginService>& plugin_service) :
    RdsHostListProvider(topology_util, plugin_service, plugin_service->GetOriginalConnAttr(), plugin_service->GetClusterId()) {}

RdsHostListProvider::RdsHostListProvider(
    const std::shared_ptr<TopologyUtil>& topology_util,
    const std::shared_ptr<PluginService>& plugin_service,
    std::map<std::string, std::string> conn_attr,
    std::string cluster_id)
    : topology_util_{ topology_util },
      plugin_service_{ plugin_service },
      conn_attr_{ conn_attr },
    HostListProvider(cluster_id)
{
    this->initial_host_info_ = HostInfo(
        conn_attr.contains(KEY_SERVER) ?
            conn_attr.at(KEY_SERVER) : "",
        conn_attr.contains(KEY_PORT) ?
            static_cast<int>(std::strtol(conn_attr.at(KEY_PORT).c_str(), nullptr, 0)) : HostInfo::NO_PORT
    );
    this->template_host_info_= HostInfo(
        RdsUtils::GetRdsInstanceHostPattern(this->initial_host_info_.GetHost()),
        this->initial_host_info_.GetPort()
    );
    this->conn_attr_.insert_or_assign(KEY_MONITORING_CONN_UUID, VALUE_BOOL_TRUE);
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

std::vector<HostInfo> RdsHostListProvider::ForceRefresh(bool verify_writer, std::chrono::milliseconds timeout_ms) {
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
            this->plugin_service_, this->topology_util_, this->conn_attr_, this->cluster_id_, this->initial_host_info_, this->template_host_info_
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
