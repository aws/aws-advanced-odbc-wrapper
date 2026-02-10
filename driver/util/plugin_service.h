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

#ifndef PLUGIN_SERVICE_H_
#define PLUGIN_SERVICE_H_
#include "sliding_cache_map.h"

#include "../host_list_providers/host_list_provider.h"
#include "../host_list_providers/topology_util.h"
#include "../host_selector/host_selector.h"

#include <map>
#include <set>

struct DBC;

struct HostFilter {
    std::set<std::string> allowed_host_ids;
    std::set<std::string> blocked_host_ids;
    std::string endpoint_type;

    bool operator==(const HostFilter& other) const {
        return allowed_host_ids == other.allowed_host_ids && blocked_host_ids == other.blocked_host_ids && endpoint_type == other.endpoint_type;
    }
};

class PluginService {
   public:
    PluginService() = default;
    PluginService(const std::shared_ptr<RdsLibLoader>& lib_loader, std::map<std::string, std::string> original_conn_attr, std::string original_conn_str);
    ~PluginService();

    virtual std::string GetClusterId();
    virtual std::string GetOriginalConnStr();
    virtual std::map<std::string, std::string> GetOriginalConnAttr();

    virtual HostInfo GetCurrentHostInfo();
    virtual HostInfo GetInitialHostInfo();
    virtual HostInfo GetTemplateHostInfo();
    virtual void SetCurrentHostInfo(const HostInfo& info);
    virtual void SetInitialHostInfo(const HostInfo& info);
    virtual void SetTemplateHostInfo(const HostInfo& info);

    virtual std::shared_ptr<HostSelector> GetHostSelector();
    virtual std::shared_ptr<Dialect> GetDialect();
    virtual std::shared_ptr<OdbcHelper> GetOdbcHelper();
    virtual std::shared_ptr<TopologyUtil> GetTopologyUtil();
    virtual std::shared_ptr<HostListProvider> GetHostListProvider();

    virtual void RefreshHosts();
    virtual void ForceRefreshHosts(bool verify_writer, uint32_t timeout_ms);

    virtual std::vector<HostInfo> GetHosts();
    virtual void SetHosts(const std::vector<HostInfo>& hosts);
    virtual std::vector<HostInfo> GetFilteredHosts();
    virtual void SetHostFilter(const HostFilter& filter);

    virtual BasePlugin* GetPluginChain();
    virtual void SetPluginChain(BasePlugin* plugin_chain);

    virtual void InitHostListProvider();

    static std::shared_ptr<HostSelector> InitHostSelector(const std::map<std::string, std::string>& conn_info);
    static std::string InitClusterId(std::map<std::string, std::string>& conn_info);
    static std::shared_ptr<Dialect> InitDialect(const std::map<std::string, std::string>& conn_info);

   private:
    std::string cluster_id_;
    std::string original_conn_str_;
    std::map<std::string, std::string> original_conn_attr_;

    HostInfo current_host_;
    HostInfo initial_host_;
    HostInfo template_host_;

    std::shared_ptr<HostSelector> host_selector_;
    std::shared_ptr<Dialect> dialect_;
    std::shared_ptr<OdbcHelper> odbc_helper_;
    std::shared_ptr<TopologyUtil> topology_util_;
    std::shared_ptr<HostListProvider> host_list_provider_;

    BasePlugin* plugin_chain_;

    // Shared resources
    // SlidingCacheMap internally thread safe
    static inline std::shared_ptr<SlidingCacheMap<std::string, std::vector<HostInfo>>> topology_map_ =
        std::make_shared<SlidingCacheMap<std::string, std::vector<HostInfo>>>();
    static inline std::shared_ptr<SlidingCacheMap<std::string, HostFilter>> host_filter_map_ =
        std::make_shared<SlidingCacheMap<std::string, HostFilter>>();
};

#endif  // PLUGIN_SERVICE_H_
