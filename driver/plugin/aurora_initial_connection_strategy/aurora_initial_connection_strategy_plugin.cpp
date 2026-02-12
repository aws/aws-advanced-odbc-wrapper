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

#include "../../host_selector/host_selector.h"
#include "../../host_selector/round_robin_host_selector.h"
#include "../../util/connection_string_keys.h"
#include "../../util/map_utils.h"
#include "../../util/rds_utils.h"

AuroraInitialConnectionStrategyPlugin::AuroraInitialConnectionStrategyPlugin(DBC* dbc) : AuroraInitialConnectionStrategyPlugin(dbc, nullptr) {}

AuroraInitialConnectionStrategyPlugin::AuroraInitialConnectionStrategyPlugin(DBC* dbc, BasePlugin* next_plugin) : BasePlugin(dbc, next_plugin) {
    this->plugin_name = "INITIAL_CONNECTION";
    const std::map<std::string, std::string> conn_info = dbc->conn_attr;

    this->plugin_service_ = dbc->plugin_service;
    this->dialect_ = plugin_service_->GetDialect();
    this->host_selector_ = plugin_service_->GetHostSelector();
    this->odbc_helper_ = plugin_service_->GetOdbcHelper();
    this->topology_util_ = plugin_service_->GetTopologyUtil();

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
    SQLUSMALLINT   DriverCompletion)
{
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
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

    if (RdsUtils::IsRdsWriterClusterDns(host) || this->verify_initial_connection_type_ == "WRITER") {
        return this->GetVerifiedWriter(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    if (RdsUtils::IsRdsReaderClusterDns(host) || this->verify_initial_connection_type_ == "READER") {
        return this->GetVerifiedReader(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

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
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const std::chrono::time_point<std::chrono::steady_clock> end_time = std::chrono::steady_clock::now() + this->retry_timeout_ms_;
    while (std::chrono::steady_clock::now() < end_time) {
        SQLRETURN rc = SQL_ERROR;

        HostInfo writer_candidate = topology_util_->GetWriter(this->plugin_service_->GetHosts());
        if (writer_candidate.GetHost().empty() || RdsUtils::IsRdsClusterDns(writer_candidate.GetHost())) {
            LOG(WARNING) << "Could not find valid writer host. Attempting connection with default server properties.";
            rc = next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
            if (!SQL_SUCCEEDED(rc)) {
                if (this->dialect_->IsSqlStateNetworkError(odbc_helper_->GetSqlState(dbc))) {
                    LOG(WARNING) << "Failed connection due to network error. Retrying connection.";
                    odbc_helper_->Disconnect(dbc);
                    std::this_thread::sleep_for(retry_delay_ms_);
                    continue;
                }
                odbc_helper_->Disconnect(dbc);
                return rc;
            }
            this->plugin_service_->ForceRefreshHosts(false, 0);
            writer_candidate = plugin_service_->GetHostListProvider()->GetConnectionInfo(dbc);

            if (writer_candidate.GetHost().empty() || writer_candidate.GetHostRole() != WRITER) {
                LOG(WARNING) << "Candidate writer connection is invalid. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }

            plugin_service_->SetInitialHostInfo(writer_candidate);

            return rc;
        }

        dbc->conn_attr.insert_or_assign(KEY_SERVER, writer_candidate.GetHost());
        rc = next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

        if (!SQL_SUCCEEDED(rc)) {
            if (this->dialect_->IsSqlStateNetworkError(odbc_helper_->GetSqlState(dbc))) {
                LOG(WARNING) << "Failed connection due to network error. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }
            odbc_helper_->Disconnect(dbc);
        }
        return rc;
    }

    return next_plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
}

SQLRETURN AuroraInitialConnectionStrategyPlugin::GetVerifiedReader(
SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const std::chrono::time_point<std::chrono::steady_clock> end_time = std::chrono::steady_clock::now() + this->retry_timeout_ms_;

    std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_REGION, "");
    if (region.empty()) {
        region = dbc->conn_attr.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
            : "";
    }

    while (std::chrono::steady_clock::now() < end_time) {
        SQLRETURN rc = SQL_ERROR;

        HostInfo reader_candidate = this->GetReader(region);
        if (reader_candidate.GetHost().empty() || RdsUtils::IsRdsClusterDns(reader_candidate.GetHost())) {
            LOG(WARNING) << "Could not find valid reader host. Connecting with default server properties.";
            rc = next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
            if (!SQL_SUCCEEDED(rc)) {
                if (this->dialect_->IsSqlStateNetworkError(odbc_helper_->GetSqlState(dbc))) {
                    LOG(WARNING) << "Failed connection due to network error. Retrying connection.";
                    odbc_helper_->Disconnect(dbc);
                    std::this_thread::sleep_for(retry_delay_ms_);
                    continue;
                }
                odbc_helper_->Disconnect(dbc);
                return rc;
            }
            this->plugin_service_->ForceRefreshHosts(false, 0);
            reader_candidate = plugin_service_->GetHostListProvider()->GetConnectionInfo(dbc);

            if (reader_candidate.GetHost().empty()) {
                LOG(WARNING) << "Candidate reader connection is invalid. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }

            if (reader_candidate.GetHostRole() != READER) {
                if (this->HasNoReaders()) {
                    // It seems that cluster has no readers. Simulate Aurora reader cluster endpoint logic
                    // and return the current (writer) connection.
                    LOG(WARNING) << "Unable to find a reader host. Fallback to writer host.";
                    this->plugin_service_->SetInitialHostInfo(reader_candidate);
                    return rc;
                }
                LOG(WARNING) << "Candidate reader connection is invalid. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }

            this->plugin_service_->SetInitialHostInfo(reader_candidate);
            return rc;
        }
        dbc->conn_attr.insert_or_assign(KEY_SERVER, reader_candidate.GetHost());
        rc = next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
        if (!SQL_SUCCEEDED(rc)) {
            if (this->dialect_->IsSqlStateNetworkError(odbc_helper_->GetSqlState(dbc))) {
                LOG(WARNING) << "Failed connection due to network error. Retrying connection.";
                odbc_helper_->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }
            odbc_helper_->Disconnect(dbc);
            return rc;
        }

        if (this->plugin_service_->GetHostListProvider()->GetConnectionRole(static_cast<SQLHDBC>(dbc)) != READER) {
            this->plugin_service_->ForceRefreshHosts(false, 0);
            if (this->HasNoReaders()) {
                // It seems that cluster has no readers. Simulate Aurora reader cluster endpoint logic
                // and return the current (writer) connection.
                LOG(WARNING) << "Unable to find a reader host. Fallback to writer host.";
                this->plugin_service_->SetInitialHostInfo(reader_candidate);
                return rc;
            }
            LOG(WARNING) << "Candidate reader connection is invalid. Retrying connection.";
            odbc_helper_->Disconnect(dbc);
            std::this_thread::sleep_for(retry_delay_ms_);
            continue;
        }

        this->plugin_service_->SetInitialHostInfo(reader_candidate);
        return rc;
    }
    return next_plugin->Connect(dbc, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
}

HostInfo AuroraInitialConnectionStrategyPlugin::GetReader(const std::string region) {

    std::vector<HostInfo> hosts = this->plugin_service_->GetHosts();
    std::vector<HostInfo> filtered_hosts;

    if (region.empty()) {
        filtered_hosts = hosts;
    } else {
        std::ranges::copy_if(hosts, std::back_inserter(filtered_hosts), [&](const HostInfo& host) {
            return RdsUtils::GetRdsRegion(host.GetHost()) == region;
        });
    }
    std::unordered_map<std::string, std::string> properties;
    RoundRobinHostSelector::SetRoundRobinWeight(filtered_hosts, properties);

    try {
        return this->host_selector_->GetHost(filtered_hosts, false, properties);
    } catch (const std::exception& e) {
        return HostInfo();
    }
}

bool AuroraInitialConnectionStrategyPlugin::HasNoReaders() {
    if (this->plugin_service_->GetHosts().empty()) {
        // Topology inconclusive/corrupted.
        return false;
    }

    for (const HostInfo& host : this->plugin_service_->GetHosts()) {
        if (host.GetHostRole() == WRITER) {
            continue;
        }
        // Found a reader node
        return false;
    }

    // Went through all hosts and found no reader.
    return true;
}
