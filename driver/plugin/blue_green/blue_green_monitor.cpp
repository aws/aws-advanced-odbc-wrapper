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

#include "blue_green_monitor.h"

#include "../../host_list_providers/rds_host_list_provider.h"

#include "../../util/rds_utils.h"

#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdio.h>
    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    #ifdef __STDC_LIB_EXT1__
        #define __STDC_WANT_LIB_EXT1__ 1
        #include <string.h> // memset_s
    #endif  /* __STDC_WANT_LIB_EXT1__ */
#endif

BlueGreenMonitor::BlueGreenMonitor(
    PluginService* plugin_service,
    BlueGreenRole initial_role,
    std::string blue_green_id,
    HostInfo initial_host_info,
    std::map<std::string, std::string> conn_attr,
    std::unordered_map<BlueGreenIntervalRate, std::chrono::milliseconds> check_interval_map,
    std::function<void(BlueGreenRole, BlueGreenInterimStatus)> on_status_change_function) :
    plugin_service_{ plugin_service },
    odbc_helper_{ plugin_service->GetOdbcHelper() },
    current_role_{ initial_role },
    blue_green_id_{ blue_green_id },
    initial_host_info_{ initial_host_info },
    conn_attr_{ conn_attr },
    check_interval_map_{ check_interval_map },
    on_status_change_function_{ on_status_change_function }
{
    dialect_blue_green_ = std::dynamic_pointer_cast<DialectBlueGreen>(plugin_service->GetDialect());
    // Create ENV local to monitor
    odbc_helper_->AllocEnv(&henv_);
    ENV* henv = static_cast<ENV*>(henv_);
    const RdsLibResult res = odbc_helper_->BaseAllocEnv(henv);
    if (!SQL_SUCCEEDED(res.fn_result)) {
        LOG(ERROR) << "Blue Green Monitor failed to create new Underlying Environment";
    }
    odbc_helper_->SetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    henv->driver_lib_loader = odbc_helper_->GetLibLoader();
}

BlueGreenMonitor::~BlueGreenMonitor() {
    bool expected = true;
    is_running_.compare_exchange_strong(expected, false);
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    std::lock_guard<std::mutex> lock_guard(hdbc_mutex_);
    odbc_helper_->DisconnectAndFree(&hdbc_);
    odbc_helper_->FreeEnv(&henv_);
}

void BlueGreenMonitor::StartMonitoring() {
    bool expected = false;
    if (is_running_.compare_exchange_strong(expected, true) && !this->monitoring_thread_) {
        plugin_head_ = this->plugin_service_->GetPluginChain();
        this->monitoring_thread_ = std::make_shared<std::thread>(&BlueGreenMonitor::Run, this);
    }
}

void BlueGreenMonitor::SetIntervalRate(BlueGreenIntervalRate interval_rate) {
    this->interval_rate_.store(interval_rate);
    this->NotifyChanges();
}

void BlueGreenMonitor::SetCollectIp(bool collect_ip) {
    this->collect_ip_.store(collect_ip);
}

void BlueGreenMonitor::SetCollectTopology(bool collect_topology) {
    this->collect_topology_.store(collect_topology);
}

void BlueGreenMonitor::SetUseIp(bool use_ip) {
    this->use_ip_.store(use_ip);
}

void BlueGreenMonitor::ResetCollectedData() {
    this->initial_ip_host_map_.clear();
    this->initial_topology_.clear();
    this->host_names_.clear();
}

void BlueGreenMonitor::SetStop(bool stop) {
    this->is_running_.store(stop);
    this->NotifyChanges();
}

void BlueGreenMonitor::Run() {
    while (is_running_.load()) {
        std::lock_guard<std::mutex> lock_guard(hdbc_mutex_);
        BlueGreenPhase old_phase = this->current_phase_;
        this->OpenConnection();
        this->CollectStatus();
        this->CollectTopology();
        this->CollectHostIp();
        this->UpdateIpAddressFlags();

        BlueGreenPhase current_phase = this->current_phase_;
        if (current_phase != BlueGreenPhase::UNKNOWN
            && (old_phase.GetPhase() == BlueGreenPhase::UNKNOWN || old_phase.GetPhase() != current_phase.GetPhase())
        ) {
            LOG(INFO) << "Status changed to: " << current_phase.ToString();
        }

        if (this->on_status_change_function_) {
            this->on_status_change_function_(
                this->current_role_,
                BlueGreenInterimStatus(
                    this->current_phase_,
                    this->current_version_,
                    this->current_port_,
                    this->initial_topology_,
                    this->current_topology_,
                    this->initial_ip_host_map_,
                    this->current_ip_host_map_,
                    this->host_names_,
                    this->all_start_topology_ip_changed_,
                    this->all_start_topology_endpoints_removed_,
                    this->all_topology_changed_
                )
            );
        }

        BlueGreenIntervalRate rate = this->in_panic_mode_ ? BlueGreenIntervalRate::HIGH : this->interval_rate_.load();
        std::chrono::milliseconds delay_ms = check_interval_map_.contains(rate) ?
            check_interval_map_.at(rate) : this->DEFAULT_INTERVAL_MS;
        this->Delay(delay_ms);
    }
}

void BlueGreenMonitor::Delay(std::chrono::milliseconds delay_ms) {
    std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point end_time = start_time + delay_ms;
    std::chrono::milliseconds minimum_delay = delay_ms < MINIMUM_DELAY_MS ? delay_ms : MINIMUM_DELAY_MS;

    BlueGreenIntervalRate current_interval_rate = this->interval_rate_;
    bool current_panic = this->in_panic_mode_;

    std::unique_lock<std::mutex> lock_guard(this->sleep_mutex_);
    do {
        this->sleep_cv_.wait_for(lock_guard, minimum_delay);
    } while (
        this->interval_rate_ == current_interval_rate
        && std::chrono::system_clock::now() < end_time
        && this->is_running_
        && current_panic == this->in_panic_mode_
    );
}

void BlueGreenMonitor::CollectHostIp() {
    this->current_ip_host_map_.clear();

    {
        if (!this->host_names_.empty()) {
            for (std::string host : host_names_) {
                this->current_ip_host_map_.try_emplace(host, this->GetIpAddress(host));
            }
        }
    }

    if (collect_ip_) {
        this->initial_ip_host_map_.clear();
        this->initial_ip_host_map_ = this->current_ip_host_map_;
    }
}

void BlueGreenMonitor::UpdateIpAddressFlags() {
    if (collect_topology_) {
        all_start_topology_ip_changed_ = false;
        all_start_topology_endpoints_removed_ = false;
        all_topology_changed_ = false;
    }

    auto initial_end = initial_ip_host_map_.end();
    auto current_end = current_ip_host_map_.end();

    if (!this->collect_ip_) {
        // All hosts in initial topology should resolve to different IP address.
        this->all_start_topology_ip_changed_ = !this->initial_topology_.empty()
            && std::all_of(this->initial_topology_.begin(), this->initial_topology_.end(),
                [&](HostInfo info)
            {
                std::string host = info.GetHost();
                auto itr_initial = initial_ip_host_map_.find(host);
                auto itr_current = current_ip_host_map_.find(host);

                return itr_initial != initial_end
                    && itr_current != current_end
                    && itr_initial->second != itr_current->second;
            });
    }

    // All hosts in initial topology should have no IP address. That means that host endpoint
    // couldn't be resolved since DNS entry doesn't exist anymore.
    this->all_start_topology_endpoints_removed_ = !this->initial_topology_.empty()
        && std::all_of(this->initial_topology_.begin(), this->initial_topology_.end(),
                [&](HostInfo info)
            {
                std::string host = info.GetHost();
                auto itr_initial = initial_ip_host_map_.find(host);
                auto itr_current = current_ip_host_map_.find(host);

                return itr_initial != initial_end
                    && itr_current == current_end;
            });

    if (this->collect_topology_) {
        // All hosts in current topology should have no same host in initial topology.
        // All hosts in current topology should have changed.
        std::set<std::string> initial_topology_nodes;
        for (HostInfo host : this->initial_topology_) {
            initial_topology_nodes.insert(host.GetHost());
        }

        std::vector<HostInfo> current_topology(this->current_topology_);
        this->all_topology_changed_ = !current_topology.empty()
            && !initial_topology_nodes.empty()
            && std::none_of(current_topology.begin(), current_topology.end(),
            [&](HostInfo info) {
                return initial_topology_nodes.contains(info.GetHost());
            });
    }
}

std::string BlueGreenMonitor::GetIpAddress(std::string host) {
    if (host.empty()) {
        LOG(ERROR) << "Empty host given to retrieve IP.";
        return {};
    }

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif // _WIN32
    int status;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    char ipstr[INET_ADDRSTRLEN];

    ClearMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host.c_str(), NULL, &hints, &servinfo)) != 0) {
        LOG(ERROR) << "Failed to retrieve IP address from host: " << host;
        return {};
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        void* addr;

        struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
        addr = &(ipv4->sin_addr);
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
    }

    if (servinfo != NULL) {
        freeaddrinfo(servinfo);
        servinfo = NULL;
    }

    return std::string(ipstr);
}

void BlueGreenMonitor::CollectTopology() {
    if (!this->host_list_provider_
        || this->hdbc_ == SQL_NULL_HDBC
        || this->odbc_helper_->IsClosed(this->hdbc_)
    ) {
        return;
    }

    std::vector<HostInfo> current_topology = this->host_list_provider_->ForceRefresh(false, std::chrono::milliseconds(30).count());
    if (!current_topology.empty()) {
        this->current_topology_ = current_topology;
        if (this->collect_topology_) {
            this->initial_topology_ = current_topology;
            {
                for (HostInfo host : current_topology) {
                    this->host_names_.insert(host.GetHost());
                }
            }
        }
    }
}

void BlueGreenMonitor::CollectStatus() {
    if (this->hdbc_ == SQL_NULL_HDBC || this->odbc_helper_->IsClosed(this->hdbc_)) {
        return;
    }

    SQLHSTMT stmt = SQL_NULL_HANDLE;
    DBC* dbc = static_cast<DBC*>(this->hdbc_);
    RdsLibResult res = this->odbc_helper_->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);
    if (SQL_SUCCEEDED(res.fn_result)) {
        res = this->odbc_helper_->ExecDirect(&stmt, this->dialect_blue_green_->GetBlueGreenStatusAvailableQuery());
        if (!SQL_SUCCEEDED(res.fn_result)) {
            this->odbc_helper_->BaseFreeStmt(&stmt);
            if (!this->odbc_helper_->IsClosed(this->hdbc_)) {
                this->current_phase_ = BlueGreenPhase::NOT_CREATED;
            } else {
                this->odbc_helper_->DisconnectAndFree(&(this->hdbc_));
                this->hdbc_ = SQL_NULL_HDBC;
                this->current_phase_ = BlueGreenPhase::UNKNOWN;
                this->in_panic_mode_.store(true);
            }
            return;
        }
        this->odbc_helper_->CloseCursor(stmt);
    }

    std::vector<StatusInfo> status_entries;

    res = this->odbc_helper_->ExecDirect(&stmt, this->dialect_blue_green_->GetBlueGreenStatusQuery());

    SQLLEN len = 0;
    SQLTCHAR version[BUFFER_SIZE] = {0};
    SQLTCHAR endpoint[BUFFER_SIZE] = {0};
    SQLTCHAR role[BUFFER_SIZE] = {0};
    SQLTCHAR status[BUFFER_SIZE] = {0};
    SQLINTEGER port = 0;

    this->odbc_helper_->BindCol(&stmt, 2, SQL_C_TCHAR, &endpoint, sizeof(endpoint), &len);
    this->odbc_helper_->BindCol(&stmt, 3, SQL_INTEGER, &port, sizeof(port), &len);
    this->odbc_helper_->BindCol(&stmt, 4, SQL_C_TCHAR, &role, sizeof(role), &len);
    this->odbc_helper_->BindCol(&stmt, 5, SQL_C_TCHAR, &status, sizeof(status), &len);
    this->odbc_helper_->BindCol(&stmt, 6, SQL_C_TCHAR, &version, sizeof(version), &len);
    res = this->odbc_helper_->Fetch(&stmt);
    while (SQL_SUCCEEDED(res.fn_result)) {
        std::string version_str = AS_UTF8_CSTR(version);
        std::string endpoint_str = AS_UTF8_CSTR(endpoint);
        int port_ = static_cast<int>(port);
        BlueGreenRole role_ = BlueGreenRole::ParseRole(AS_UTF8_CSTR(role), version_str);
        BlueGreenPhase phase_ = BlueGreenPhase::ParsePhase(AS_UTF8_CSTR(status), version_str);

        if (this->current_role_ == role_) {
            status_entries.push_back({version_str, endpoint_str, port_, phase_, role_});
        }
        res = this->odbc_helper_->Fetch(&stmt);
    }
    this->odbc_helper_->BaseFreeStmt(&stmt);

    auto itr_end = status_entries.end();
    auto itr = std::find_if(
        status_entries.begin(),
        status_entries.end(),
        [](StatusInfo info) {
            return RdsUtils::IsRdsWriterClusterDns(info.endpoint) && RdsUtils::IsNotOldInstance(info.endpoint);
        });

    if (itr != itr_end) {
        // Found cluster writer endpoint, add in read-only endpoint to host names as well
        std::string converted_endpoint = itr->endpoint;
        std::transform(converted_endpoint.begin(), converted_endpoint.end(), converted_endpoint.begin(), [](unsigned char c) { return std::tolower(c); });
        std::string search = ".cluster-";
        size_t begin_idx = converted_endpoint.find(search);
        if (begin_idx != std::string::npos) {
            converted_endpoint = converted_endpoint.replace(begin_idx, begin_idx + search.length(), ".cluster-ro-");
        }
        this->host_names_.insert(converted_endpoint);
    } else {
        // Update iterator to check if the endpoint is an instance endpoint
        itr = std::find_if(
            status_entries.begin(),
            status_entries.end(),
            [](StatusInfo info) {
                return RdsUtils::IsRdsInstance(info.endpoint) && RdsUtils::IsNotOldInstance(info.endpoint);
            });
    }

    if (itr != itr_end) {
        this->current_phase_ = itr->phase;
        this->current_version_ = itr->version;
        this->current_port_ = itr->port;
    } else if (status_entries.empty()) {
        // It's normal to expect that the status table has no entries after BGD is completed.
        // Old1 cluster/instance has been separated and no longer receives updates from related green cluster/instance.
        // Metadata at new blue cluster/instance can be removed after switchover, and it's also expected to get
        // no records.
        if (this->current_role_.GetRole() != BlueGreenRole::SOURCE) {
            LOG(INFO) << "No entries in status table.";
        }
        this->current_phase_ = BlueGreenPhase::UNKNOWN;
    }

    if (collect_topology_) {
        for (StatusInfo info : status_entries) {
            if (!info.endpoint.empty() && RdsUtils::IsNotOldInstance(info.endpoint)) {
                std::string converted_endpoint = info.endpoint;
                std::transform(converted_endpoint.begin(), converted_endpoint.end(), converted_endpoint.begin(), [](unsigned char c) { return std::tolower(c); });
                this->host_names_.insert(converted_endpoint);
            }
        }
    }

    if (!this->connecting_host_info_correct_ && itr != itr_end) {
        // Connected to initial host, need to ensure this is desired cluster
        std::string endpoint_ip = this->GetIpAddress(itr->endpoint);
        std::string connected_ip = this->connecting_ip_;
        if (!connected_ip.empty() && connected_ip != endpoint_ip) {
            // Need to reconnect as not connected to desired cluster
            this->connecting_host_info_ = HostInfo(itr->endpoint, itr->port);
            this->odbc_helper_->DisconnectAndFree(&(this->hdbc_));
            this->in_panic_mode_.store(true);
        } else {
            this->in_panic_mode_.store(false);
        }
        this->connecting_host_info_correct_.store(true);
    }

    if (this->connecting_host_info_correct_ && this->host_list_provider_) {
        this->InitHostListProvider();
    }
}

void BlueGreenMonitor::OpenConnection() {
    if (!this->odbc_helper_->IsClosed(this->hdbc_)) {
        return;
    }

    this->odbc_helper_->DisconnectAndFree(&(this->hdbc_));
    this->in_panic_mode_.store(true);

    SQLHDBC local_hdbc;
    odbc_helper_->AllocDbc(henv_, local_hdbc);
    DBC *local_dbc = static_cast<DBC*>(local_hdbc);
    local_dbc->conn_attr = this->conn_attr_;

    if (this->connecting_host_info_.GetHostRole() == HOST_ROLE::UNKNOWN) {
        this->connecting_host_info_ = this->initial_host_info_;
        this->connecting_ip_ = "";
        this->connecting_host_info_correct_ = false;
    }

    HostInfo connecting_host_info = this->connecting_host_info_;
    std::string connecting_ip = this->connecting_ip_;

    if (this->use_ip_.load() && !connecting_ip.empty()) {
        local_dbc->conn_attr.insert_or_assign(KEY_IAM_HOST, connecting_host_info.GetHost());
        local_dbc->conn_attr.insert_or_assign(KEY_SERVER, connecting_ip);
    }

    SQLRETURN rc = this->plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    if (SQL_SUCCEEDED(rc)) {
        this->connecting_ip_ = this->GetIpAddress(connecting_host_info.GetHost());
        this->hdbc_ = local_hdbc;
        this->in_panic_mode_ = false;
    } else {
        odbc_helper_->DisconnectAndFree(&local_hdbc);
    }
    this->NotifyChanges();
}

void BlueGreenMonitor::NotifyChanges() {
    std::lock_guard<std::mutex> lock_guard(sleep_mutex_);
    sleep_cv_.notify_all();
}

void BlueGreenMonitor::InitHostListProvider() {
    if (!this->host_list_provider_ || !this->connecting_host_info_correct_ || this->connecting_host_info_.GetHost().empty()) {
        return;
    }

    // Special cluster ID to differentiate between other host list providers for this cluster
    std::string custom_cluster_id = this->blue_green_id_ + this->current_role_.ToString() + this->BG_CLUSTER_ID;

    std::map<std::string, std::string> host_list_provider_conn_attr(this->conn_attr_);
    host_list_provider_conn_attr.insert_or_assign(KEY_CLUSTER_ID, custom_cluster_id);
    host_list_provider_conn_attr.insert_or_assign(KEY_SERVER, this->connecting_host_info_.GetHost());
    host_list_provider_conn_attr.insert_or_assign(KEY_PORT, std::to_string(this->connecting_host_info_.GetPort()));
    this->host_list_provider_ = std::make_shared<RdsHostListProvider>(
        this->plugin_service_->GetTopologyUtil(),
        this->plugin_service_,
        host_list_provider_conn_attr,
        custom_cluster_id
    );
}

void BlueGreenMonitor::ClearMemory(void* dest, size_t count) {
#ifdef _WIN32
    SecureZeroMemory(dest, count);
#else
    #ifdef __STDC_LIB_EXT1__
        memset_s(dest, count, '\0', count);
    #else
        memset(dest, '\0', count);
    #endif // __STDC_LIB_EXT1__
#endif // _WIN32
}
