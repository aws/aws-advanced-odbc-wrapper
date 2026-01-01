#include "topology_service.h"

#include <algorithm>

#include "connection_string_keys.h"
#include "rds_utils.h"

TopologyService::TopologyService(std::string cluster_id) : cluster_id(cluster_id) {}

std::vector<HostInfo> TopologyService::GetHosts() {
    return topology_map_->Get(this->cluster_id);
}

void TopologyService::SetHosts(std::vector<HostInfo> hosts) {
    topology_map_->Put(this->cluster_id, hosts);
}

std::vector<HostInfo> TopologyService::GetFilteredHosts() {
    std::vector<HostInfo> hosts = topology_map_->Get(this->cluster_id);
    if (host_filter.allowed_host_ids.empty() && host_filter.blocked_host_ids.empty()) {
        return hosts;
    }

    std::vector<HostInfo> filtered_hosts;
    std::copy_if(hosts.begin(), hosts.end(), std::back_inserter(filtered_hosts),
        [&](HostInfo host) {
            std::string host_name = host.GetHost();
            return host_filter.allowed_host_ids.contains(host_name)
                && !host_filter.blocked_host_ids.contains(host_name);
        }
    );
    return filtered_hosts;
}

void TopologyService::SetHostFilter(HostFilter filter) {
    this->host_filter = filter;
}

std::string TopologyService::InitClusterId(std::map<std::string, std::string>& conn_info) {
    std::string generated_id;
    if (conn_info.contains(KEY_CLUSTER_ID)) {
        generated_id = conn_info.at(KEY_CLUSTER_ID);
    } else {
        generated_id = RdsUtils::GetRdsClusterId(conn_info.at(KEY_SERVER));
        if (generated_id.empty()) {
            generated_id = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        }
        LOG(INFO) << "ClusterId generated and set to: " << generated_id;
        conn_info.insert_or_assign(KEY_CLUSTER_ID, generated_id);
    }
    return generated_id;
}
