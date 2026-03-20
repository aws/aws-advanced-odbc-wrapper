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

#include "blue_green_status_provider.h"

#include "routing/connect/reject_connect_routing.h"
#include "routing/connect/substitute_connect_routing.h"
#include "routing/connect/suspend_connect_routing.h"
#include "routing/connect/suspend_until_node_found_connect_routing.h"
#include "routing/execute/suspend_execute_routing.h"

#include "../../util/map_utils.h"
#include "../../util/rds_utils.h"

#include <algorithm>
#include <format>
#include <functional>
#include <ranges>

std::hash<std::string> BlueGreenStatusProvider::hasher;

BlueGreenStatusProvider::BlueGreenStatusProvider(
    std::shared_ptr<PluginService> plugin_service,
    std::map<std::string, std::string> conn_attr,
    std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> status_cache,
    std::string blue_green_id, std::string cluster_id)
    : plugin_service_{ plugin_service },
    conn_attr_{ conn_attr },
    status_cache_{ status_cache },
    blue_green_id_{ blue_green_id },
    cluster_id_{ cluster_id }
{
    this->phase_time_map_ = std::make_shared<ConcurrentMap<std::string, PhaseTimeInfo>>();
    this->host_ip_map_ = std::make_shared<ConcurrentMap<std::string, std::string>>();
    this->corresponding_nodes_ = std::make_shared<ConcurrentMap<std::string, std::pair<HostInfo, HostInfo>>>();
    this->role_by_host_map_ = std::make_shared<ConcurrentMap<std::string, BlueGreenRole>>();
    this->iam_host_success_connects_map_ = std::make_shared<ConcurrentMap<std::string, std::set<std::string>>>();
    this->green_node_change_name_times_map_ = std::make_shared<ConcurrentMap<std::string, std::chrono::system_clock::time_point>>();
    this->check_interval_map_.insert_or_assign(BlueGreenIntervalRate::BASELINE,
        MapUtils::GetMillisecondsValue(this->conn_attr_, KEY_BG_BASELINE_REFRESH_MS, BASELINE_MS));
    this->check_interval_map_.insert_or_assign(BlueGreenIntervalRate::INCREASED,
        MapUtils::GetMillisecondsValue(this->conn_attr_, KEY_BG_INCREASED_REFRESH_MS, INCREASED_MS));
    this->check_interval_map_.insert_or_assign(BlueGreenIntervalRate::HIGH,
        MapUtils::GetMillisecondsValue(this->conn_attr_, KEY_BG_HIGH_REFRESH_MS, HIGH_MS));
    this->switchover_timeout_ms_ = MapUtils::GetMillisecondsValue(this->conn_attr_, KEY_BG_SWITCH_TIMEOUT_MS, SWITCHOVER_TIMEOUT_MS);
}

BlueGreenStatusProvider::~BlueGreenStatusProvider() {
    std::lock_guard monitor_lock(this->monitor_mutex_);
    std::shared_ptr<BlueGreenMonitor> source_monitor = this->monitors_[BlueGreenRole::SOURCE];
    std::shared_ptr<BlueGreenMonitor> target_monitor = this->monitors_[BlueGreenRole::TARGET];
    if (source_monitor != nullptr) {
        source_monitor->Stop();
        this->monitors_[BlueGreenRole::SOURCE] = nullptr;
    }
    if (target_monitor != nullptr) {
        target_monitor->Stop();
        this->monitors_[BlueGreenRole::TARGET] = nullptr;
    }
}

void BlueGreenStatusProvider::InitMonitoring() {
    std::lock_guard monitor_lock(this->monitor_mutex_);
    if (monitors_[BlueGreenRole::SOURCE] && monitors_[BlueGreenRole::TARGET]) {
        return;
    }

    std::shared_ptr<BlueGreenMonitor> source_monitor = std::make_shared<BlueGreenMonitor>(
        this->plugin_service_,
        BlueGreenRole::SOURCE,
        blue_green_id_,
        this->plugin_service_->GetCurrentHostInfo(),
        GetMonitoringProperties(),
        check_interval_map_,
        [this](BlueGreenRole role, BlueGreenInterimStatus interim_status) { this->PrepareStatus(role, interim_status); });
    source_monitor->StartMonitoring();
    monitors_[BlueGreenRole::SOURCE] = source_monitor;

    std::shared_ptr<BlueGreenMonitor> target_monitor = std::make_shared<BlueGreenMonitor>(
        this->plugin_service_,
        BlueGreenRole::TARGET,
        blue_green_id_,
        this->plugin_service_->GetCurrentHostInfo(),
        GetMonitoringProperties(),
        check_interval_map_,
        [this](BlueGreenRole role, BlueGreenInterimStatus interim_status) { this->PrepareStatus(role, interim_status); });
    target_monitor->StartMonitoring();
    monitors_[BlueGreenRole::TARGET] = target_monitor;
}

std::map<std::string, std::string> BlueGreenStatusProvider::GetMonitoringProperties() {
    std::map<std::string, std::string> conn_attr_copy(this->conn_attr_);
    conn_attr_copy.insert_or_assign(KEY_MONITORING_CONN_UUID, VALUE_BOOL_TRUE);
    return conn_attr_copy;
}

void BlueGreenStatusProvider::PrepareStatus(BlueGreenRole role, BlueGreenInterimStatus interim_status) {
    std::lock_guard<std::mutex> lock_guard(this->process_lock_guard);

    // Detect changes
    int status_hash = interim_status.GetHashCode();
    int context_hash = this->GetContextHash();
    if (this->interim_status_hashes_[role.GetRole()] == status_hash
        && this->last_context_hash_ == context_hash)
    {
        return;
    }

    this->UpdatePhase(role, interim_status);
    this->interim_statuses_[role.GetRole()] = interim_status;
    this->interim_status_hashes_[role.GetRole()] = status_hash;
    this->last_context_hash_ = context_hash;

    this->host_ip_map_->CopyMap(interim_status.initial_ip_host_map_);

    for (std::string host : interim_status.host_names_) {
        this->role_by_host_map_->InsertOrAssign(host, role);
    }

    this->UpdateCorrespondingNodes();
    this->UpdateSummaryStatus(role, interim_status);
    this->UpdateMonitors();
    this->UpdateStatusCache();
    this->LogCurrentContext();
    this->LogSwitchoverFinalSummary();
    this->ResetContextWhenCompleted();
}

void BlueGreenStatusProvider::UpdatePhase(BlueGreenRole role, BlueGreenInterimStatus interim_status) {
    BlueGreenInterimStatus role_status = interim_statuses_[role.GetRole()];
    BlueGreenPhase latest_interim_phase = role_status.phase_ == BlueGreenPhase::UNKNOWN ? BlueGreenPhase::NOT_CREATED : role_status.phase_;

    if (role == BlueGreenRole::TARGET
        && latest_interim_phase != BlueGreenPhase::UNKNOWN
        && interim_status.phase_ != BlueGreenPhase::UNKNOWN
        && latest_interim_phase != BlueGreenPhase::COMPLETED
        && interim_status.phase_ < latest_interim_phase)
    {
        this->rollback_ = true;
    }

    if (interim_status.phase_ == BlueGreenPhase::UNKNOWN) {
        return;
    }

    if (!this->rollback_) {
        if (interim_status.phase_ >= this->latest_status_phase_) {
            this->latest_status_phase_ = interim_status.phase_;
        }
    } else if (interim_status.phase_ < this->latest_status_phase_) {
        this->latest_status_phase_ = interim_status.phase_;
    }
}

void BlueGreenStatusProvider::UpdateStatusCache() {
    LOG(INFO) << "Updating status cache for: " << this->blue_green_id_ << ", to: " << this->summary_status_.ToString();
    this->status_cache_->InsertOrAssign(this->blue_green_id_, this->summary_status_);
    this->StorePhaseTime(this->summary_status_.GetCurrentPhase());
}

void BlueGreenStatusProvider::UpdateCorrespondingNodes() {
    this->corresponding_nodes_->Clear();

    BlueGreenInterimStatus source_interim_status = this->interim_statuses_[BlueGreenRole::SOURCE];
    BlueGreenInterimStatus target_interim_status = this->interim_statuses_[BlueGreenRole::TARGET];
    if (source_interim_status.phase_ != BlueGreenPhase::UNKNOWN
        && !source_interim_status.initial_topology_.empty()
        && target_interim_status.phase_ != BlueGreenPhase::UNKNOWN
        && !target_interim_status.initial_topology_.empty())
    {
        HostInfo blue_writer_host = GetWriterHost(BlueGreenRole::SOURCE);
        HostInfo green_writer_host = GetWriterHost(BlueGreenRole::TARGET);
        std::vector<HostInfo> blue_reader_hosts = GetReaderHosts(BlueGreenRole::SOURCE);
        std::vector<HostInfo> green_reader_hosts = GetReaderHosts(BlueGreenRole::TARGET);

        if (!blue_writer_host.GetHost().empty()) {
            // green_writer_host can be null but that will be handled properly by corresponding routing.
            this->corresponding_nodes_->InsertOrAssign(blue_writer_host.GetHost(), {blue_writer_host, green_writer_host});
        }

        if (!blue_reader_hosts.empty()) {
            // Map blue readers to green nodes
            if (!green_reader_hosts.empty()) {
                int green_idx = 0;
                for (HostInfo host : blue_reader_hosts) {
                    this->corresponding_nodes_->InsertOrAssign(host.GetHost(), {host, green_reader_hosts.at(green_idx++)});
                    green_idx %= green_reader_hosts.size();
                }
            } else {
                // No green readers, map blue readers to green writer
                for (HostInfo host : blue_reader_hosts) {
                    this->corresponding_nodes_->InsertOrAssign(host.GetHost(), {host, green_writer_host});
                }
            }
        }
    }

    if (source_interim_status.phase_ != BlueGreenPhase::UNKNOWN
        && !source_interim_status.host_names_.empty()
        && target_interim_status.phase_ != BlueGreenPhase::UNKNOWN
        && !target_interim_status.host_names_.empty())
    {
        std::set<std::string> blue_hosts = source_interim_status.host_names_;
        std::set<std::string> green_hosts = target_interim_status.host_names_;

        auto itr_blue = std::find_if(blue_hosts.begin(), blue_hosts.end(), RdsUtils::IsRdsClusterDns);
        auto itr_green = std::find_if(green_hosts.begin(), green_hosts.end(), RdsUtils::IsRdsClusterDns);

        std::string blue_cluster_host = itr_blue != blue_hosts.end() ? *itr_blue : "";
        std::string green_cluster_host = itr_green != green_hosts.end() ? *itr_green : "";

        if (!blue_cluster_host.empty() && !green_cluster_host.empty()) {
            this->corresponding_nodes_->TryEmplace(blue_cluster_host, {HostInfo(blue_cluster_host), HostInfo(green_cluster_host)});
        }

        itr_blue = std::find_if(blue_hosts.begin(), blue_hosts.end(), RdsUtils::IsRdsReaderClusterDns);
        itr_green = std::find_if(green_hosts.begin(), green_hosts.end(), RdsUtils::IsRdsReaderClusterDns);

        std::string blue_reader_cluster_host = itr_blue != blue_hosts.end() ? *itr_blue : "";
        std::string green_reader_cluster_host = itr_green != green_hosts.end() ? *itr_green : "";

        if (!blue_cluster_host.empty() && !green_cluster_host.empty()) {
            this->corresponding_nodes_->TryEmplace(blue_reader_cluster_host, {HostInfo(blue_reader_cluster_host), HostInfo(green_reader_cluster_host)});
        }

        for (std::string host : blue_hosts) {
            std::string custom_cluster_name = RdsUtils::GetRdsClusterId(host);
            if (!custom_cluster_name.empty()) {
                auto itr = std::find_if(green_hosts.begin(), green_hosts.end(), [this, &custom_cluster_name](std::string green_host_name)
                {
                    return RdsUtils::IsRdsCustomClusterDns(green_host_name) &&
                           custom_cluster_name == RdsUtils::RemoveGreenInstancePrefix(green_host_name);
                });
                if (itr != green_hosts.end()) {
                    std::string green_host = *itr;
                    corresponding_nodes_->TryEmplace(host, { HostInfo(host), HostInfo(green_host) });
                }
            }
        }
    }
}

HostInfo BlueGreenStatusProvider::GetWriterHost(BlueGreenRole role) {
    BlueGreenInterimStatus interim_status = this->interim_statuses_[role.GetRole()];
    if (interim_status.phase_ == BlueGreenPhase::UNKNOWN) {
        return {};
    }

    std::vector<HostInfo> host_list = interim_status.initial_topology_;
    auto itr = std::find_if(host_list.begin(), host_list.end(), [](HostInfo info) { return info.GetHostRole() == HOST_ROLE::WRITER; });

    if (itr != host_list.end()) {
        return *itr;
    }

    return {};
}

std::vector<HostInfo> BlueGreenStatusProvider::GetReaderHosts(BlueGreenRole role) {
    BlueGreenInterimStatus interim_status = this->interim_statuses_[role.GetRole()];
    if (interim_status.phase_ == BlueGreenPhase::UNKNOWN) {
        return {};
    }

    std::vector<HostInfo> host_list = interim_status.initial_topology_;
    std::vector<HostInfo> readers_host_list;
    std::ranges::copy_if(host_list, std::back_inserter(readers_host_list), [](HostInfo info) { return info.GetHostRole() == HOST_ROLE::READER; });

    return readers_host_list;
}

void BlueGreenStatusProvider::UpdateSummaryStatus(BlueGreenRole role, BlueGreenInterimStatus interim_status) {
    switch (this->latest_status_phase_.GetPhase()) {
        case BlueGreenPhase::NOT_CREATED:
            this->summary_status_ = BlueGreenStatus(this->blue_green_id_, BlueGreenPhase::NOT_CREATED);
            break;
        case BlueGreenPhase::CREATED:
            this->UpdateDnsFlags(role, interim_status);
            this->summary_status_ = this->GetStatusOfCreated();
            break;
        case BlueGreenPhase::PREPARATION:
            this->StartSwitchoverTimer();
            this->UpdateDnsFlags(role, interim_status);
            this->summary_status_ = this->GetStatusOfPreparation();
            break;
        case BlueGreenPhase::IN_PROGRESS:
            this->UpdateDnsFlags(role, interim_status);
            this->summary_status_ = this->GetStatusOfInProgress();
            this->ResetMonitors(this->monitor_reset_on_in_progress_completed_, "- start");
            break;
        case BlueGreenPhase::POST:
            this->UpdateDnsFlags(role, interim_status);
            this->summary_status_ = this->GetStatusOfPost();
            break;
        case BlueGreenPhase::COMPLETED:
            this->UpdateDnsFlags(role, interim_status);
            this->summary_status_ = this->GetStatusOfCompleted();
            break;
        default:
            break;
    }
}

void BlueGreenStatusProvider::UpdateMonitors() {
    switch (this->summary_status_.GetCurrentPhase().GetPhase()) {
        case BlueGreenPhase::NOT_CREATED:
            for (BlueGreenRole role : {BlueGreenRole::SOURCE, BlueGreenRole::TARGET}) {
                if (this->monitors_[role.GetRole()]) {
                    this->monitors_[role.GetRole()]->SetIntervalRate(BlueGreenIntervalRate::BASELINE);
                    this->monitors_[role.GetRole()]->SetCollectIp(false);
                    this->monitors_[role.GetRole()]->SetCollectTopology(false);
                    this->monitors_[role.GetRole()]->SetUseIp(false);
                };
            }
            break;
        case BlueGreenPhase::CREATED:
            for (BlueGreenRole role : {BlueGreenRole::SOURCE, BlueGreenRole::TARGET}) {
                if (this->monitors_[role.GetRole()]) {
                    this->monitors_[role.GetRole()]->SetIntervalRate(BlueGreenIntervalRate::INCREASED);
                    this->monitors_[role.GetRole()]->SetCollectIp(true);
                    this->monitors_[role.GetRole()]->SetCollectTopology(true);
                    this->monitors_[role.GetRole()]->SetUseIp(false);
                    if (this->rollback_) {
                        this->monitors_[role.GetRole()]->ResetCollectedData();
                    }
                };
            }
            break;
        case BlueGreenPhase::PREPARATION:
        case BlueGreenPhase::IN_PROGRESS:
        case BlueGreenPhase::POST:
            for (BlueGreenRole role : {BlueGreenRole::SOURCE, BlueGreenRole::TARGET}) {
                if (this->monitors_[role.GetRole()]) {
                    this->monitors_[role.GetRole()]->SetIntervalRate(BlueGreenIntervalRate::HIGH);
                    this->monitors_[role.GetRole()]->SetCollectIp(false);
                    this->monitors_[role.GetRole()]->SetCollectTopology(false);
                    this->monitors_[role.GetRole()]->SetUseIp(true);
                };
            }
            break;
        case BlueGreenPhase::COMPLETED:
            for (BlueGreenRole role : {BlueGreenRole::SOURCE, BlueGreenRole::TARGET}) {
                if (this->monitors_[role.GetRole()]) {
                    this->monitors_[role.GetRole()]->SetIntervalRate(BlueGreenIntervalRate::BASELINE);
                    this->monitors_[role.GetRole()]->SetCollectIp(false);
                    this->monitors_[role.GetRole()]->SetCollectTopology(false);
                    this->monitors_[role.GetRole()]->SetUseIp(false);
                    this->monitors_[role.GetRole()]->ResetCollectedData();
                }
            }
            break;
        default:
            break;
    }
}

void BlueGreenStatusProvider::UpdateDnsFlags(BlueGreenRole role, BlueGreenInterimStatus interim_status) {
    if (role == BlueGreenRole::SOURCE
        && !this->blue_dns_update_completed_
        && interim_status.all_start_topology_ip_changed_)
    {
        LOG(INFO) << "Blue DNS Completed";
        this->blue_dns_update_completed_ = true;
        this->StoreBlueDsnUpdateTime();
    }

    if (role == BlueGreenRole::TARGET
        && !this->green_dns_removed_
        && interim_status.all_start_topology_endpoints_removed_)
    {
        LOG(INFO) << "Green DNS Removed";
        this->green_dns_removed_ = true;
        this->StoreGreenDsnRemoveTime();
    }

    if (role == BlueGreenRole::TARGET
        && !this->green_topology_changed_
        && interim_status.all_topology_changed_)
    {
        this->green_topology_changed_ = true;
        this->StoreGreenTopologyChangeTime();
        this->ResetMonitors(this->monitor_reset_on_topology_completed_, "- green topology");
    }
}

void BlueGreenStatusProvider::ResetMonitors(std::atomic<bool>& monitor_reset_completed, std::string event_name) {
    bool expected = false;
    if (monitor_reset_completed.compare_exchange_strong(expected, true)) {
        this->StoreMonitorResetTime(event_name);
    }
}

int BlueGreenStatusProvider::GetContextHash() {
    int result = this->GetValueHash(1, std::to_string(this->all_green_nodes_changed_));
    result = this->GetValueHash(result, std::to_string(this->iam_host_success_connects_map_->Size()));
    return result;
}

int BlueGreenStatusProvider::GetValueHash(int current_hash, std::string value) {
    return current_hash * 31 + hasher(value);
}

std::string BlueGreenStatusProvider::GetHostPort(std::string host, int port) {
    if (port > 0) {
        return host + ":" + std::to_string(port);
    }
    return host;
}

std::vector<std::shared_ptr<BaseConnectRouting>> BlueGreenStatusProvider::AddSubstituteBlueWithIpAddressConnectRouting() {
    std::vector<std::shared_ptr<BaseConnectRouting>> connect_routes;
    for (auto itr : this->role_by_host_map_->GetMapCopy()) {
        std::string host = itr.first;
        BlueGreenRole role = itr.second;
        std::pair<HostInfo, HostInfo> host_pair = this->corresponding_nodes_->Get(host);
        if (role != BlueGreenRole::SOURCE || host_pair.first.GetHost().empty()) {
            continue;
        }
        HostInfo blue_host = host_pair.first;
        std::string blue_ip = this->host_ip_map_->Get(blue_host.GetHost());
        HostInfo blue_ip_host = blue_ip.empty() ?
            blue_host
            : HostInfo(blue_ip, blue_host.GetPort(), blue_host.GetHostState(), blue_host.GetHostRole());

        std::vector<HostInfo> blue_ip_hosts;
        blue_ip_hosts.push_back(blue_host);
        connect_routes.push_back(std::make_shared<SubstituteConnectRouting>(host, role, blue_ip_host, blue_ip_hosts));

        BlueGreenInterimStatus interim_status = this->interim_statuses_[role.GetRole()];
        if (interim_status.phase_ == BlueGreenPhase::UNKNOWN) {
            continue;
        }

        std::vector<HostInfo> blue_interim_hosts;
        blue_interim_hosts.push_back(blue_host);
        connect_routes.push_back(std::make_shared<SubstituteConnectRouting>(this->GetHostPort(host, interim_status.port_), role, blue_ip_host, blue_interim_hosts));
    }

    return connect_routes;
}

BlueGreenStatus BlueGreenStatusProvider::GetStatusOfCreated() {
    return BlueGreenStatus(
        this->blue_green_id_,
        BlueGreenPhase::CREATED,
        std::vector<std::shared_ptr<BaseConnectRouting>>(),
        std::vector<std::shared_ptr<BaseExecuteRouting>>(),
        this->role_by_host_map_,
        this->corresponding_nodes_);
}

BlueGreenStatus BlueGreenStatusProvider::GetStatusOfPreparation() {
    // Limit switchover duration to default post status duration
    if (this->IsSwitchoverTimerExpired()) {
        LOG(INFO) << "BG Switchover Timeout";
        if (this->rollback_) {
            return this->GetStatusOfCreated();
        }
        return this->GetStatusOfCompleted();
    }

    std::vector<std::shared_ptr<BaseConnectRouting>> connect_routes = this->AddSubstituteBlueWithIpAddressConnectRouting();
    return BlueGreenStatus(
        this->blue_green_id_,
        BlueGreenPhase::PREPARATION,
        connect_routes,
        std::vector<std::shared_ptr<BaseExecuteRouting>>(),
        this->role_by_host_map_,
        this->corresponding_nodes_);
}

BlueGreenStatus BlueGreenStatusProvider::GetStatusOfInProgress() {
    if (this->IsSwitchoverTimerExpired()) {
        LOG(INFO) << "BG Switchover Timeout";
        if (this->rollback_) {
            return this->GetStatusOfCreated();
        }
        return this->GetStatusOfCompleted();
    }

    std::vector<std::shared_ptr<BaseConnectRouting>> connect_routes;
    connect_routes.push_back(std::make_shared<SuspendConnectRouting>("", BlueGreenRole::SOURCE, this->blue_green_id_));
    connect_routes.push_back(std::make_shared<SuspendConnectRouting>("", BlueGreenRole::TARGET, this->blue_green_id_));

    // All traffic should be suspended
    std::vector<std::shared_ptr<BaseExecuteRouting>> execute_routes;
    execute_routes.push_back(std::make_shared<SuspendExecuteRouting>("", BlueGreenRole::SOURCE, this->blue_green_id_));
    execute_routes.push_back(std::make_shared<SuspendExecuteRouting>("", BlueGreenRole::TARGET, this->blue_green_id_));

    // All connect calls with IP should be suspended
    std::map<std::string, std::string> host_to_ip_map = this->host_ip_map_->GetMapCopy();
    for (auto& [key, value] : host_to_ip_map) {
        // Try to confirm ip for blue nodes
        BlueGreenInterimStatus status = this->interim_statuses_[BlueGreenRole::SOURCE];
        if (status.phase_ != BlueGreenPhase::UNKNOWN) {
            std::map<std::string, std::string> ip_to_host_map = status.initial_ip_host_map_;
            auto itr = std::find_if(ip_to_host_map.begin(), ip_to_host_map.end(), [value](const std::pair<const std::string, std::string>& pair) {
                return !pair.first.empty() && value == pair.first;
            });
            if (itr != ip_to_host_map.end()) {
                // Connect
                connect_routes.push_back(std::make_shared<SuspendConnectRouting>(value, BlueGreenRole::UNKNOWN, this->blue_green_id_));
                connect_routes.push_back(std::make_shared<SuspendConnectRouting>(this->GetHostPort(value, status.port_), BlueGreenRole::UNKNOWN, this->blue_green_id_));
                // Execute
                execute_routes.push_back(std::make_shared<SuspendExecuteRouting>(value, BlueGreenRole::UNKNOWN, this->blue_green_id_));
                execute_routes.push_back(std::make_shared<SuspendExecuteRouting>(this->GetHostPort(value, status.port_), BlueGreenRole::UNKNOWN, this->blue_green_id_));
                continue;
            }
        }
        // Try to confirm ip for green nodes
        status = this->interim_statuses_[BlueGreenRole::TARGET];
        if (status.phase_ != BlueGreenPhase::UNKNOWN) {
            std::map<std::string, std::string> ip_to_host_map = status.initial_ip_host_map_;
            auto itr = std::find_if(ip_to_host_map.begin(), ip_to_host_map.end(), [value](const std::pair<const std::string, std::string>& pair) {
                return !pair.first.empty() && value == pair.first;
            });
            if (itr != ip_to_host_map.end()) {
                connect_routes.push_back(std::make_shared<SuspendConnectRouting>(value, BlueGreenRole::UNKNOWN, this->blue_green_id_));
                connect_routes.push_back(
                    std::make_shared<SuspendConnectRouting>(this->GetHostPort(value, status.port_), BlueGreenRole::UNKNOWN, this->blue_green_id_));
                execute_routes.push_back(std::make_shared<SuspendExecuteRouting>(value, BlueGreenRole::UNKNOWN, this->blue_green_id_));
                execute_routes.push_back(
                    std::make_shared<SuspendExecuteRouting>(this->GetHostPort(value, status.port_), BlueGreenRole::UNKNOWN, this->blue_green_id_));
                continue;
            }
        }
    }

    return BlueGreenStatus(this->blue_green_id_, BlueGreenPhase::IN_PROGRESS, connect_routes, execute_routes, this->role_by_host_map_,
                           this->corresponding_nodes_);
}

BlueGreenStatus BlueGreenStatusProvider::GetStatusOfPost() {
    if (this->IsSwitchoverTimerExpired()) {
        LOG(INFO) << "BG Switchover Timeout";
        if (this->rollback_) {
            return this->GetStatusOfCreated();
        }
        return this->GetStatusOfCompleted();
    }

    std::vector<std::shared_ptr<BaseConnectRouting>> connect_routes;
    std::vector<std::shared_ptr<BaseExecuteRouting>> execute_routes;
    this->CreatePostRouting(connect_routes);

    return BlueGreenStatus(
        this->blue_green_id_,
        BlueGreenPhase::POST,
        connect_routes,
        execute_routes,
        this->role_by_host_map_,
        this->corresponding_nodes_);
}

void BlueGreenStatusProvider::CreatePostRouting(std::vector<std::shared_ptr<BaseConnectRouting>>& connect_routing) {
    if (!this->blue_dns_update_completed_ || !this->all_green_nodes_changed_) {
        for (auto& [host, role] : this->role_by_host_map_->GetMapCopy()) {
            if (role != BlueGreenRole::SOURCE || !this->corresponding_nodes_->Contains(host)) {
                continue;
            }
            std::string blue_host = host;
            bool is_blue_host_instance = RdsUtils::IsRdsInstance(blue_host);

            std::pair<HostInfo, HostInfo> node_pair = this->corresponding_nodes_->Get(blue_host);
            HostInfo blue_host_info = node_pair.first;
            HostInfo green_host_info = node_pair.second;

            if (green_host_info.GetHost().empty()) {
                connect_routing.push_back(std::make_shared<SuspendUntilNodeFoundConnectRouting>(blue_host, role, this->blue_green_id_));
                BlueGreenInterimStatus interim_status = this->interim_statuses_[role.GetRole()];
                if (interim_status.phase_ != BlueGreenPhase::UNKNOWN) {
                    connect_routing.push_back(std::make_shared<SuspendUntilNodeFoundConnectRouting>(
                        this->GetHostPort(blue_host, interim_status.port_), role, this->blue_green_id_));
                }
            } else {
                std::string green_host = green_host_info.GetHost();
                std::string green_host_ip = this->host_ip_map_->Get(green_host);
                HostInfo green_ip_host_info = HostInfo(green_host_ip, green_host_info.GetPort(), green_host_info.GetHostState(), green_host_info.GetHostRole());

                // Check if green ghost connected with blue (non-prefixed) IAM host name
                std::vector<HostInfo> iam_hosts;
                HostInfo iam_blue_host_info = HostInfo(RdsUtils::RemoveGreenInstancePrefix(green_host_ip), green_host_info.GetPort(),
                                                       green_host_info.GetHostState(), green_host_info.GetHostRole());
                if (!this->IsAlreadySuccessfullyConnected(green_host, iam_blue_host_info.GetHost())) {
                    iam_hosts.push_back(green_host_info);
                }
                iam_hosts.push_back(iam_blue_host_info);

                connect_routing.push_back(std::make_shared<SubstituteConnectRouting>(
                    blue_host, role, green_ip_host_info, iam_hosts,
                    is_blue_host_instance
                        ? std::function<void(std::string)>([this, green_host](std::string iam_host) { this->RegisterIamHost(green_host, iam_host); })
                        : nullptr));

                BlueGreenInterimStatus interim_status = this->interim_statuses_[role.GetRole()];
                if (interim_status.phase_ != BlueGreenPhase::UNKNOWN) {
                    connect_routing.push_back(std::make_shared<SubstituteConnectRouting>(
                        this->GetHostPort(blue_host, interim_status.port_), role, green_ip_host_info, iam_hosts,
                        is_blue_host_instance ? std::function<void(std::string)>(
                                                    [this, green_host](std::string iam_host) { this->RegisterIamHost(green_host, iam_host); })
                                              : nullptr));
                }
            }
        }
    }

    if (!this->green_topology_changed_) {
        for (auto [host, role] : this->role_by_host_map_->GetMapCopy()) {
            if (role != BlueGreenRole::TARGET) {
                continue;
            }
            std::string green_host = host;
            bool is_green_host_instance = RdsUtils::IsRdsInstance(green_host);
            std::string blue_host = RdsUtils::RemoveGreenInstancePrefix(green_host);
            BlueGreenInterimStatus interim_status = interim_statuses_[role.GetRole()];

            HostInfo green_host_info = HostInfo(green_host, interim_status.port_);
            HostInfo blue_host_info = HostInfo(blue_host, interim_status.port_);
            HostInfo green_ip_host_info = HostInfo(this->host_ip_map_->Get(green_host), interim_status.port_);

            std::vector<HostInfo> iam_hosts;
            if (!this->IsAlreadySuccessfullyConnected(green_host, blue_host)) {
                iam_hosts.push_back(green_host_info);
            }
            iam_hosts.push_back(blue_host_info);

            connect_routing.push_back(std::make_shared<SubstituteConnectRouting>(
                green_host, role, green_ip_host_info, iam_hosts,
                is_green_host_instance
                    ? std::function<void(std::string)>([this, green_host](std::string iam_host) { this->RegisterIamHost(green_host, iam_host); })
                    : nullptr));

            if (interim_status.phase_ != BlueGreenPhase::UNKNOWN) {
                connect_routing.push_back(std::make_shared<SubstituteConnectRouting>(
                    this->GetHostPort(green_host, interim_status.port_), role, green_ip_host_info, iam_hosts,
                    is_green_host_instance
                        ? std::function<void(std::string)>([this, green_host](std::string iam_host) { this->RegisterIamHost(green_host, iam_host); })
                        : nullptr));
            }
        }
    } else if (!this->green_dns_removed_) {
        // Green topology changed, calls to green endpoint should be rejected
        connect_routing.push_back(std::make_shared<RejectConnectRouting>("", BlueGreenRole::TARGET));
    }
}

BlueGreenStatus BlueGreenStatusProvider::GetStatusOfCompleted() {
    if (this->IsSwitchoverTimerExpired()) {
        LOG(INFO) << "BG Switchover Timeout";
        if (this->rollback_) {
            return this->GetStatusOfCreated();
        }

        return BlueGreenStatus(this->blue_green_id_, BlueGreenPhase::COMPLETED, std::vector<std::shared_ptr<BaseConnectRouting>>(),
                               std::vector<std::shared_ptr<BaseExecuteRouting>>(), this->role_by_host_map_, this->corresponding_nodes_);
    }

    // BG reported completed but DNS not updated completely, pretend it is not completed
    if (!this->blue_dns_update_completed_ || !this->green_dns_removed_) {
        return this->GetStatusOfPost();
    }

    return BlueGreenStatus(this->blue_green_id_, BlueGreenPhase::COMPLETED, std::vector<std::shared_ptr<BaseConnectRouting>>(),
                           std::vector<std::shared_ptr<BaseExecuteRouting>>(), this->role_by_host_map_, this->corresponding_nodes_);
}

void BlueGreenStatusProvider::RegisterIamHost(std::string connect_host, std::string iam_host) {
    bool different_node_name = !connect_host.empty() && connect_host != iam_host;
    if (different_node_name) {
        if (!this->IsAlreadySuccessfullyConnected(connect_host, iam_host)) {
            this->green_node_change_name_times_map_->TryEmplace(connect_host, std::chrono::system_clock::now());
            LOG(INFO) << "BG Green node name changed";
        }
    }
    {
        std::lock_guard<std::mutex> lock_guard(this->iam_mutex_);
        std::set<std::string> iam_success_connects = this->iam_host_success_connects_map_->Get(connect_host);
        iam_success_connects.insert(iam_host);
        this->iam_host_success_connects_map_->InsertOrAssign(connect_host, iam_success_connects);
    }

    if (different_node_name) {
        std::map<std::string, std::set<std::string>> iam_success_connect_map = this->iam_host_success_connects_map_->GetMapCopy();
        bool all_host_changed = std::all_of(iam_success_connect_map.begin(), iam_success_connect_map.end(), [](auto& entry) {
            std::string host = entry.first;
            std::set<std::string> iam_host = entry.second;
            if (iam_host.empty()) {
                return true;
            }

            return std::any_of(iam_host.begin(), iam_host.end(), [host](std::string iam_host) { return host != iam_host; });
        });
        if (all_host_changed && !this->all_green_nodes_changed_) {
            LOG(INFO) << "BG all green nodes changed names";
            this->all_green_nodes_changed_ = true;
            this->StoreGreenNodeChangeNameTime();
        }
    }
}

bool BlueGreenStatusProvider::IsAlreadySuccessfullyConnected(std::string connect_host, std::string iam_host) {
    return this->iam_host_success_connects_map_->Get(connect_host).contains(iam_host);
}

void BlueGreenStatusProvider::StorePhaseTime(BlueGreenPhase phase) {
    if (phase == BlueGreenPhase::UNKNOWN) {
        return;
    }
    this->phase_time_map_->TryEmplace(
        phase.ToString() + (this->rollback_ ? "(rollback)" : ""),
        {.timestamp = std::chrono::system_clock::now(), .phase = phase}
    );
}

void BlueGreenStatusProvider::StoreBlueDsnUpdateTime() {
    this->phase_time_map_->TryEmplace(
        std::string("Blue DNS Updated") + (this->rollback_ ? "(rollback)" : ""),
        {.timestamp = std::chrono::system_clock::now(), .phase = BlueGreenPhase::UNKNOWN}
    );
}

void BlueGreenStatusProvider::StoreGreenDsnRemoveTime() {
    this->phase_time_map_->TryEmplace(
        std::string("Green DNS Removed") + (this->rollback_ ? "(rollback)" : ""),
        {.timestamp = std::chrono::system_clock::now(), .phase = BlueGreenPhase::UNKNOWN}
    );
}

void BlueGreenStatusProvider::StoreGreenNodeChangeNameTime() {
    this->phase_time_map_->TryEmplace(
        std::string("Green Node Certificate Changed") + (this->rollback_ ? "(rollback)" : ""),
        {.timestamp = std::chrono::system_clock::now(), .phase = BlueGreenPhase::UNKNOWN}
    );
}

void BlueGreenStatusProvider::StoreGreenTopologyChangeTime() {
    this->phase_time_map_->TryEmplace(
        std::string("Green Topology Changed") + (this->rollback_ ? "(rollback)" : ""),
        {.timestamp = std::chrono::system_clock::now(), .phase = BlueGreenPhase::UNKNOWN}
    );
}

void BlueGreenStatusProvider::StoreMonitorResetTime(std::string event_name) {
    this->phase_time_map_->TryEmplace(
        std::string("Monitor reset: ") + event_name + (this->rollback_ ? "(rollback)" : ""),
        {.timestamp = std::chrono::system_clock::now(), .phase = BlueGreenPhase::UNKNOWN}
    );
}

void BlueGreenStatusProvider::LogSwitchoverFinalSummary() {
    bool switchover_complete =
        (!this->rollback_ && this->summary_status_.GetCurrentPhase() == BlueGreenPhase::COMPLETED)
        || (this->rollback_ && this->summary_status_.GetCurrentPhase() == BlueGreenPhase::CREATED);

    std::map<std::string, PhaseTimeInfo> phase_time = this->phase_time_map_->GetMapCopy();
    bool has_active_switchover = std::any_of(phase_time.begin(), phase_time.end(), [](const std::pair<const std::string, PhaseTimeInfo>& pair) {
        return pair.second.phase.IsSwitchoverOrCompleted();
    });

    if (!switchover_complete || !has_active_switchover) {
        return;
    }

    int event_name_left_pad = 5;
    int event_name_default_size = 31;
    int max_event_name_size = event_name_default_size;
    for (auto [key, _] : phase_time) {
        if (key.length() + event_name_left_pad > max_event_name_size) {
            max_event_name_size = key.length() + event_name_left_pad;
        }
    }

    BlueGreenPhase time_zero_phase = this->rollback_ ? BlueGreenPhase::PREPARATION : BlueGreenPhase::IN_PROGRESS;
    std::string time_zero_key = this->rollback_ ? time_zero_phase.ToString() + " (rollback)" : time_zero_phase.ToString();
    std::chrono::system_clock::time_point time_zero =
        phase_time.contains(time_zero_key) ? phase_time.at(time_zero_key).timestamp : std::chrono::system_clock::time_point{};
    std::string divider = std::string(52, '-') + std::string(max_event_name_size, '-') + "\n";
    std::string format_header = "{:<28} {:>21} {:>" + std::to_string(max_event_name_size) + "}\n";

    std::vector<std::pair<std::string, PhaseTimeInfo>> sorted_phase(phase_time.begin(), phase_time.end());
    std::ranges::sort(sorted_phase, [](const auto& a, const auto& b) { return a.second.timestamp < b.second.timestamp; });

    std::ostringstream out_stream;
    for (const auto& [name, phase_time] : sorted_phase) {
        auto since_zero = phase_time.timestamp.time_since_epoch() - time_zero.time_since_epoch();

        out_stream << std::fixed
            << std::setw(28) << std::left
            << static_cast<int64_t>(phase_time.timestamp.time_since_epoch().count())
            << std::setw(18) << std::right
            << static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(since_zero).count())
            << " ms "
            << std::setw(max_event_name_size) << std::left
            << name;
        out_stream << "\n";
    }

    std::string log_message = std::format("[bgId: '{}']\n", this->blue_green_id_) + divider +
                              std::vformat(format_header, std::make_format_args("timestamp", "time offset (ms)", "event")) + divider +
                              out_stream.str() + divider;
    LOG(INFO) << log_message;
}

void BlueGreenStatusProvider::ResetContextWhenCompleted() {
    bool switchover_complete = (!this->rollback_ && this->summary_status_.GetCurrentPhase() == BlueGreenPhase::COMPLETED) ||
                               (this->rollback_ && this->summary_status_.GetCurrentPhase() == BlueGreenPhase::CREATED);

    std::map<std::string, PhaseTimeInfo> phase_time = this->phase_time_map_->GetMapCopy();
    bool has_active_switchover = std::any_of(phase_time.begin(), phase_time.end(), [](const std::pair<const std::string, PhaseTimeInfo>& pair) {
        return pair.second.phase.IsSwitchoverOrCompleted();
    });

    if (switchover_complete && has_active_switchover) {
        {
            std::lock_guard monitor_lock(this->monitor_mutex_);
            std::shared_ptr<BlueGreenMonitor> source_monitor = this->monitors_[BlueGreenRole::SOURCE];
            std::shared_ptr<BlueGreenMonitor> target_monitor = this->monitors_[BlueGreenRole::TARGET];
            if (source_monitor != nullptr) {
                source_monitor->Stop();
                this->monitors_[BlueGreenRole::SOURCE] = nullptr;
            }
            if (target_monitor != nullptr) {
                target_monitor->Stop();
                this->monitors_[BlueGreenRole::TARGET] = nullptr;
            }
        }
        this->rollback_ = false;
        this->summary_status_ = BlueGreenStatus();
        this->latest_status_phase_ = BlueGreenPhase::NOT_CREATED;
        this->phase_time_map_->Clear();
        this->blue_dns_update_completed_ = false;
        this->green_dns_removed_ = false;
        this->green_topology_changed_ = false;
        this->all_green_nodes_changed_ = false;
        this->post_status_end_time_ = std::chrono::system_clock::time_point{};
        this->interim_status_hashes_[BlueGreenRole::SOURCE] = 0;
        this->interim_status_hashes_[BlueGreenRole::TARGET] = 0;
        this->last_context_hash_ = 0;
        this->interim_statuses_[BlueGreenRole::SOURCE] = BlueGreenInterimStatus();
        this->interim_statuses_[BlueGreenRole::TARGET] = BlueGreenInterimStatus();
        this->host_ip_map_->Clear();
        this->corresponding_nodes_->Clear();
        this->role_by_host_map_->Clear();
        this->iam_host_success_connects_map_->Clear();
        this->green_node_change_name_times_map_->Clear();
        this->monitor_reset_on_in_progress_completed_ = false;
        this->monitor_reset_on_topology_completed_ = false;

        this->InitMonitoring();
    }
}

void BlueGreenStatusProvider::StartSwitchoverTimer() {
    if (this->post_status_end_time_ == std::chrono::system_clock::time_point{}) {
        this->post_status_end_time_ = std::chrono::system_clock::now() + this->switchover_timeout_ms_;
    }
}

bool BlueGreenStatusProvider::IsSwitchoverTimerExpired() {
    return this->post_status_end_time_ > std::chrono::system_clock::time_point{}
        && this->post_status_end_time_ < std::chrono::system_clock::now();
}

void BlueGreenStatusProvider::LogCurrentContext() {
    std::ostringstream corresponding_nodes_str;
    for (auto [key, value] : this->corresponding_nodes_->GetMapCopy()) {
        corresponding_nodes_str << key << ", " << value.second.GetHostPortPair() << "\n";
    }
    std::ostringstream phase_time_str;
    for (auto [key, value] : this->phase_time_map_->GetMapCopy()) {
        phase_time_str << key << ", " << value.timestamp << "\n";
    }
    std::ostringstream green_name_change_str;
    for (auto [key, value] : this->green_node_change_name_times_map_->GetMapCopy()) {
        green_name_change_str << key << ", " << value << "\n";
    }

    std::ostringstream log_msg;
    log_msg << std::format("[bgId: {}] Summary Status:\n{}", this->blue_green_id_, this->summary_status_.ToString());
    log_msg << std::format("Corresponding Nodes:\n{}", corresponding_nodes_str.str());
    log_msg << std::format("Phase Times:\n{}", phase_time_str.str());
    log_msg << std::format("Green Node Certificate Change Times:\n{}", green_name_change_str.str());
    log_msg << std::format("Latest Status Phase: {}\n", this->latest_status_phase_.ToString());
    log_msg << std::format("Blue DNS Update Completed: {}\n", std::to_string(this->blue_dns_update_completed_));
    log_msg << std::format("Green Node Changed Name: {}\n", std::to_string(this->all_green_nodes_changed_));
    log_msg << std::format("Green Topology Changed: {}\n", std::to_string(this->green_topology_changed_));

    LOG(INFO) << log_msg.str();
}
