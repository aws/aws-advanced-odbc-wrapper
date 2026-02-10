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

#include "plugin_service.h"

#include "rds_utils.h"

#include "../dialect/dialect.h"
#include "../dialect/dialect_aurora_mysql.h"
#include "../dialect/dialect_aurora_postgres.h"

#include "../host_selector/highest_weight_host_selector.h"
#include "../host_selector/random_host_selector.h"
#include "../host_selector/round_robin_host_selector.h"

#include "../host_list_providers/aurora_topology_util.h"
#include "../host_list_providers/host_list_provider.h"
#include "../host_list_providers/rds_host_list_provider.h"

PluginService::PluginService(const std::shared_ptr<RdsLibLoader>& lib_loader, std::map<std::string, std::string> original_conn_attr, std::string original_conn_str) :
    original_conn_str_{ std::move(original_conn_str) },
    original_conn_attr_{ std::move(original_conn_attr) }
{
    this->initial_host_ = HostInfo(
        original_conn_attr_.contains(KEY_SERVER) ?
            original_conn_attr_.at(KEY_SERVER) : "",
        original_conn_attr_.contains(KEY_PORT) ?
            static_cast<int>(std::strtol(original_conn_attr_.at(KEY_PORT).c_str(), nullptr, 0)) : HostInfo::NO_PORT
    );
    this->template_host_ = HostInfo(
        RdsUtils::GetRdsInstanceHostPattern(this->initial_host_.GetHost()),
        this->initial_host_.GetPort()
    );
    this->cluster_id_ = InitClusterId(original_conn_attr_);
    this->host_selector_ = InitHostSelector(original_conn_attr_);
    this->dialect_ = InitDialect(original_conn_attr_);
    this->odbc_helper_ = std::make_shared<OdbcHelper>(lib_loader);
    this->topology_util_ = std::make_shared<AuroraTopologyUtil>(this->odbc_helper_, this->dialect_);
}

PluginService::~PluginService()
{
    host_list_provider_ = nullptr;
    topology_util_ = nullptr;
    odbc_helper_ = nullptr;
    dialect_ = nullptr;
    host_selector_ = nullptr;
}

std::string PluginService::GetClusterId() {
    return this->cluster_id_;
}

std::string PluginService::GetOriginalConnStr() {
    return this->original_conn_str_;
}

std::map<std::string, std::string> PluginService::GetOriginalConnAttr() {
    return this->original_conn_attr_;
}

HostInfo PluginService::GetCurrentHostInfo() {
    return this->current_host_;
}

HostInfo PluginService::GetInitialHostInfo() {
    return this->initial_host_;
}

HostInfo PluginService::GetTemplateHostInfo() {
    return this->template_host_;
}

void PluginService::SetCurrentHostInfo(const HostInfo& info) {
    this->current_host_ = info;
}

void PluginService::SetInitialHostInfo(const HostInfo& info) {
    this->initial_host_ = info;
}

void PluginService::SetTemplateHostInfo(const HostInfo& info) {
    this->template_host_ = info;
}

std::shared_ptr<HostSelector> PluginService::GetHostSelector() {
    return this->host_selector_;
}

std::shared_ptr<Dialect> PluginService::GetDialect() {
    return this->dialect_;
}

std::shared_ptr<OdbcHelper> PluginService::GetOdbcHelper() {
    return this->odbc_helper_;
}

std::shared_ptr<TopologyUtil> PluginService::GetTopologyUtil() {
    return this->topology_util_;
}

std::shared_ptr<HostListProvider> PluginService::GetHostListProvider() {
    return this->host_list_provider_;
}

void PluginService::RefreshHosts() {
    const std::vector<HostInfo> new_hosts = this->host_list_provider_->Refresh();
    this->SetHosts(new_hosts);
}

void PluginService::ForceRefreshHosts(bool verify_writer, uint32_t timeout_ms) {
    const std::vector<HostInfo> new_hosts = this->host_list_provider_->ForceRefresh(verify_writer, timeout_ms);
    this->SetHosts(new_hosts);
}

std::vector<HostInfo> PluginService::GetHosts() {
    return topology_map_->Get(this->cluster_id_);
}

void PluginService::SetHosts(const std::vector<HostInfo>& hosts) {
    topology_map_->Put(this->cluster_id_, hosts);
}

std::vector<HostInfo> PluginService::GetFilteredHosts() {
    std::vector<HostInfo> hosts = topology_map_->Get(this->cluster_id_);
    HostFilter host_filter = host_filter_map_->Get(this->cluster_id_);

    if (host_filter.allowed_host_ids.empty() && host_filter.blocked_host_ids.empty()) {
        return hosts;
    }

    std::vector<HostInfo> filtered_hosts;
    std::copy_if(hosts.begin(), hosts.end(), std::back_inserter(filtered_hosts),
        [&](const HostInfo& host) {
            const std::string host_id = host.GetHostId();
            if (!host_filter.allowed_host_ids.empty()) {
                return host_filter.allowed_host_ids.contains(host_id);
            }
            if (!host_filter.blocked_host_ids.empty()) {
                return !(host_filter.endpoint_type == "READER" && host.IsHostWriter())
                    && !host_filter.blocked_host_ids.contains(host_id);
            }
            return true;
        }
    );
    return filtered_hosts;
}

void PluginService::SetHostFilter(const HostFilter& filter) {
    host_filter_map_->Put(this->cluster_id_, filter);
}

BasePlugin* PluginService::GetPluginChain() {
    return this->plugin_chain_;
}

void PluginService::SetPluginChain(BasePlugin* plugin_chain) {
    this->plugin_chain_ = plugin_chain;
}

void PluginService::InitHostListProvider() {
    switch (this->dialect_->GetDialectType()) {
        case DatabaseDialectType::AURORA_POSTGRESQL:
        case DatabaseDialectType::AURORA_POSTGRESQL_LIMITLESS:
        case DatabaseDialectType::AURORA_MYSQL:
            this->host_list_provider_ = std::make_shared<RdsHostListProvider>(this->topology_util_, this);
            break;
        default:
            this->host_list_provider_ = std::make_shared<HostListProvider>(this->cluster_id_);
    }
}

std::shared_ptr<HostSelector> PluginService::InitHostSelector(const std::map<std::string, std::string>& conn_info) {
    HostSelectorStrategies selector_strategy = RANDOM_HOST;
    if (conn_info.contains(KEY_HOST_SELECTOR_STRATEGY)) {
        selector_strategy = HostSelector::GetHostSelectorStrategy(conn_info.at(KEY_HOST_SELECTOR_STRATEGY));
    }

    switch (selector_strategy) {
        case ROUND_ROBIN:
            return std::make_shared<RoundRobinHostSelector>();
        case HIGHEST_WEIGHT:
            return std::make_shared<HighestWeightHostSelector>();
        case RANDOM_HOST:
        case UNKNOWN_STRATEGY:
        default:
            return std::make_shared<RandomHostSelector>();
    }
}

std::string PluginService::InitClusterId(std::map<std::string, std::string>& conn_info) {
    std::string generated_id;
    if (conn_info.contains(KEY_CLUSTER_ID)) {
        generated_id = conn_info.at(KEY_CLUSTER_ID);
    } else {
        generated_id = RdsUtils::GetRdsClusterId(conn_info.contains(KEY_SERVER) ? conn_info.at(KEY_SERVER) : "");
        if (generated_id.empty()) {
            generated_id = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        }
        LOG(INFO) << "ClusterId generated and set to: " << generated_id;
        conn_info.insert_or_assign(KEY_CLUSTER_ID, generated_id);
    }
    return generated_id;
}

std::shared_ptr<Dialect> PluginService::InitDialect(const std::map<std::string, std::string>& conn_info) {
    DatabaseDialectType dialect = DatabaseDialectType::UNKNOWN_DIALECT;
    if (conn_info.contains(KEY_DATABASE_DIALECT)) {
        dialect = Dialect::DatabaseDialectFromString(conn_info.at(KEY_DATABASE_DIALECT));
    }

    if (dialect == DatabaseDialectType::UNKNOWN_DIALECT) {
        // TODO - Dialect from host
        // For release, we are only supporting Aurora PostgreSQL and Aurora PostgreSQL Limitless
        const std::string host = conn_info.contains(KEY_SERVER) ? conn_info.at(KEY_SERVER) : "";

        if (RdsUtils::IsLimitlessDbShardGroupDns(host)) {
            dialect = DatabaseDialectType::AURORA_POSTGRESQL_LIMITLESS;
        } else {
            dialect = DatabaseDialectType::AURORA_POSTGRESQL;
        }
    }

    switch (dialect) {
        case DatabaseDialectType::AURORA_POSTGRESQL:
            return std::make_shared<DialectAuroraPostgres>();
        case DatabaseDialectType::AURORA_POSTGRESQL_LIMITLESS:
            return std::make_shared<DialectAuroraPostgresLimitless>();
        case DatabaseDialectType::AURORA_MYSQL:
            return std::make_shared<DialectAuroraMySql>();
        default:
            return std::make_shared<Dialect>();
    }
}
