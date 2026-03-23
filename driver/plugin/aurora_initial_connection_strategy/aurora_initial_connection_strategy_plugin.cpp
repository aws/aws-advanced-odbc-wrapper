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

#include "aurora_initial_connection_strategy_plugin.h"

#include <algorithm>
#include <thread>
#include <iostream>

#include "../../host_selector/host_selector.h"
#include "../../host_selector/round_robin_host_selector.h"
#include "../../util/connection_string_keys.h"
#include "../../util/map_utils.h"
#include "../../util/rds_utils.h"

AuroraInitialConnectionStrategyPlugin::AuroraInitialConnectionStrategyPlugin(DBC* dbc) : AuroraInitialConnectionStrategyPlugin(dbc, nullptr) {}

AuroraInitialConnectionStrategyPlugin::AuroraInitialConnectionStrategyPlugin(DBC* dbc, BasePlugin* next_plugin) : AuroraInitialConnectionStrategyPlugin(
    dbc,
    next_plugin,
    dbc->plugin_service,
    dbc->plugin_service->GetHostSelector(),
    dbc->plugin_service->GetDialect(),
    dbc->plugin_service->GetOdbcHelper(),
    dbc->plugin_service->GetTopologyUtil()) {}

AuroraInitialConnectionStrategyPlugin::AuroraInitialConnectionStrategyPlugin(
    DBC* dbc,
    BasePlugin* next_plugin,
    std::shared_ptr<PluginService> plugin_service,
    std::shared_ptr<HostSelector> host_selector,
    std::shared_ptr<Dialect> dialect,
    std::shared_ptr<OdbcHelper> odbc_helper,
    std::shared_ptr<TopologyUtil> topology_util) : BasePlugin(dbc, next_plugin) {

    this->plugin_name = "INITIAL_CONNECTION";
    const std::map<std::string, std::string> conn_info = dbc->conn_attr;

    this->plugin_service_ = plugin_service;
    this->dialect_ = dialect;
    this->host_selector_ = host_selector;
    this->odbc_helper_ = odbc_helper;
    this->topology_util_ = topology_util;

    this->retry_delay_ms_ = MapUtils::GetMillisecondsValue(conn_info, KEY_INITIAL_CONNECTION_RETRY_INTERVAL_MS, DEFAULT_INITIAL_CONNECTION_RETRY_INTERVAL_MS);
    this->retry_timeout_ms_ = MapUtils::GetMillisecondsValue(conn_info, KEY_INITIAL_CONNECTION_RETRY_TIMEOUT_MS, DEFAULT_INITIAL_CONNECTION_RETRY_TIMEOUT_MS);
    this->verify_initial_connection_type_ = MapUtils::GetStringValue(conn_info, KEY_VERIFY_INITIAL_CONNECTION_TYPE, "");

}

SQLRETURN AuroraInitialConnectionStrategyPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion) {
    const DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const std::map<std::string, std::string> conn_info = dbc->conn_attr;
    const std::string host = MapUtils::GetStringValue(conn_info, KEY_SERVER, "");
    if (!RdsUtils::IsRdsClusterDns(host)) {
        LOG(WARNING) << "Non-RdsClusterDns detected. Bypassing Aurora Initial Connection Strategy plugin.";
        return next_plugin->Connect(
            ConnectionHandle,
            WindowHandle,
            OutConnectionString,
            BufferLength,
            StringLengthPtr,
            DriverCompletion);
    }

    if (plugin_service_->GetHostListProvider() == nullptr) {
        plugin_service_->InitHostListProvider();
        plugin_service_->RefreshHosts();
    }


    std::cout << "Connect - hostlist size:" << plugin_service_->GetHosts().size() << std::endl;
    if (plugin_service_->GetHosts().size() < 1) {
        plugin_service_->ForceRefreshHosts(false, 0);
    }

    if (verify_initial_connection_type_ == "WRITER") {
        return this->GetVerifiedWriter(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    if (verify_initial_connection_type_ == "READER") {
        return this->GetVerifiedReader(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    if (RdsUtils::IsRdsWriterClusterDns(host)) {
        return this->GetVerifiedWriter(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    if (RdsUtils::IsRdsReaderClusterDns(host)) {
        return this->GetVerifiedReader(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    LOG(WARNING) << "Unable to determine connection type. Attempting connection with default connection parameters.";
    return next_plugin->Connect(
        ConnectionHandle,
        WindowHandle,
        OutConnectionString,
        BufferLength,
        StringLengthPtr,
        DriverCompletion);
}

SQLRETURN AuroraInitialConnectionStrategyPlugin::GetVerifiedWriter(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    std::cout << "GetVerifiedWriter" << std::endl;
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const std::chrono::time_point<std::chrono::steady_clock> end_time = std::chrono::steady_clock::now() + retry_timeout_ms_;
    while (std::chrono::steady_clock::now() < end_time) {
        SQLRETURN rc = SQL_ERROR;

        std::cout << "GetVerifiedWriter - hostlist size:" << plugin_service_->GetHosts().size() << std::endl;
        HostInfo writer_candidate = topology_util_->GetWriter(plugin_service_->GetHosts());
        std::cout << "WriterCandidate:"<< writer_candidate.GetHost().c_str() << std::endl;
        if (writer_candidate.GetHost().empty()) {
            LOG(WARNING) << "Could not find valid writer host. Attempting connection with default connection parameters.";
            rc = next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
            if (!SQL_SUCCEEDED(rc)) {
                if (dialect_->IsSqlStateNetworkError(odbc_helper_->GetSqlStateAndLogMessage(dbc).c_str())) {
                    LOG(WARNING) << "Failed connection due to network error. Retrying connection.";
                    odbc_helper_->Disconnect(dbc);
                    std::this_thread::sleep_for(retry_delay_ms_);
                    continue;
                }
                odbc_helper_->Disconnect(dbc);
                return rc;
            }
            plugin_service_->ForceRefreshHosts(false, 0);
            writer_candidate = plugin_service_->GetHostListProvider()->GetConnectionInfo(dbc);

            if (writer_candidate.GetHost().empty() || writer_candidate.GetHostRole() != WRITER) {
                LOG(WARNING) << "Candidate writer connection is invalid. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }

            return rc;
        }

        dbc->conn_attr.insert_or_assign(KEY_SERVER, writer_candidate.GetHost());
        rc = next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

        if (!SQL_SUCCEEDED(rc)) {
            if (dialect_->IsSqlStateNetworkError(odbc_helper_->GetSqlStateAndLogMessage(dbc).c_str())) {
                LOG(WARNING) << "Failed connection due to network error. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }
            odbc_helper_->Disconnect(dbc);
        }
        return rc;
    }
    LOG(WARNING) << "Retry timeout exceeded and unable to find a writer host. Attempting connection with default connection parameters.";
    return next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
}

SQLRETURN AuroraInitialConnectionStrategyPlugin::GetVerifiedReader(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    std::cout << "GetVerifiedReader" << std::endl;
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const std::chrono::time_point<std::chrono::steady_clock> end_time = std::chrono::steady_clock::now() + retry_timeout_ms_;

    std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_REGION, "");
    if (region.empty()) {
        region = dbc->conn_attr.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
            : "";
    }

    while (std::chrono::steady_clock::now() < end_time) {
        SQLRETURN rc = SQL_ERROR;

        std::cout << "GetVerifiedReader - hostlist size:" << plugin_service_->GetHosts().size() << std::endl;
        HostInfo reader_candidate = this->GetReader(region);
        std::cout << "ReaderCandidate:"<< reader_candidate.GetHost().c_str() << std::endl;
        if (reader_candidate.GetHost().empty()) {
            LOG(WARNING) << "Could not find valid reader host. Connecting with default server properties.";
            rc = next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
            if (!SQL_SUCCEEDED(rc)) {
                if (dialect_->IsSqlStateNetworkError(odbc_helper_->GetSqlStateAndLogMessage(dbc).c_str())) {
                    LOG(WARNING) << "Failed connection due to network error. Retrying connection.";
                    odbc_helper_->Disconnect(dbc);
                    std::this_thread::sleep_for(retry_delay_ms_);
                    continue;
                }
                odbc_helper_->Disconnect(dbc);
                return rc;
            }
            plugin_service_->ForceRefreshHosts(false, 0);
            reader_candidate = plugin_service_->GetHostListProvider()->GetConnectionInfo(dbc);

            if (reader_candidate.GetHost().empty()) {
                LOG(WARNING) << "Candidate reader connection is invalid. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }

            if (reader_candidate.GetHostRole() != READER) {
                if (this->HasNoReadersAndTopologyIsHealthy()) {
                    // It seems that cluster has no readers. Simulate Aurora reader cluster endpoint logic
                    // and return the current (writer) connection.
                    LOG(WARNING) << "Unable to find a reader host. Attempting connection with default connection parameters.";
                    return rc;
                }
                LOG(WARNING) << "Candidate reader connection is invalid. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }

            return rc;
        }
        LOG(INFO) << "Connecting to reader host: " << reader_candidate.GetHost();
        dbc->conn_attr.insert_or_assign(KEY_SERVER, reader_candidate.GetHost());
        rc = next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
        if (!SQL_SUCCEEDED(rc)) {
            if (dialect_->IsSqlStateNetworkError(odbc_helper_->GetSqlStateAndLogMessage(dbc).c_str())) {
                LOG(WARNING) << "Failed connection due to network error. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }
            odbc_helper_->Disconnect(dbc);
            return rc;
        }

        if (plugin_service_->GetHostListProvider()->GetConnectionRole(static_cast<SQLHDBC>(dbc)) != READER) {
            plugin_service_->ForceRefreshHosts(false, 0);
            if (this->HasNoReadersAndTopologyIsHealthy()) {
                // It seems that cluster has no readers. Simulate Aurora reader cluster endpoint logic
                // and return the current (writer) connection.
                LOG(WARNING) << "Unable to find a reader host. Attempting connection with default connection parameters.";
                return rc;
            }
            LOG(WARNING) << "Candidate reader connection is invalid. Retrying connection.";
            odbc_helper_->Disconnect(dbc);
            std::this_thread::sleep_for(retry_delay_ms_);
            continue;
        }

        return rc;
    }
    LOG(WARNING) << "Retry timeout exceeded and unable to find a reader host. Using default connection parameters.";
    return next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
}

HostInfo AuroraInitialConnectionStrategyPlugin::GetReader(const std::string region) {
    std::cout << "GetReader" << std::endl;
    std::vector<HostInfo> hosts = plugin_service_->GetHosts();
    std::vector<HostInfo> filtered_hosts;

    if (region.empty()) {
        filtered_hosts = hosts;
    } else {
        std::ranges::copy_if(hosts, std::back_inserter(filtered_hosts), [&](const HostInfo& host) {
            return RdsUtils::GetRdsRegion(host.GetHost()) == region && host.GetHostRole() == READER;
        });
    }
    std::unordered_map<std::string, std::string> properties;
    RoundRobinHostSelector::SetRoundRobinWeight(filtered_hosts, properties);

    try {
        return host_selector_->GetHost(filtered_hosts, false, properties);
    } catch (const std::exception& e) {
        std::cout << "GetReader Failed - hostselector exception: " << e.what() << std::endl;
        return {};
    }
}

bool AuroraInitialConnectionStrategyPlugin::HasNoReadersAndTopologyIsHealthy() {
    if (plugin_service_->GetHosts().empty()) {
        // Topology inconclusive/corrupted.
        return false;
    }

    if (std::ranges::all_of(plugin_service_->GetHosts(), [](const HostInfo& host) {return host.GetHostRole() == WRITER;})) {
        return false;
    }

    // Went through all hosts and found no reader.
    return true;
}
