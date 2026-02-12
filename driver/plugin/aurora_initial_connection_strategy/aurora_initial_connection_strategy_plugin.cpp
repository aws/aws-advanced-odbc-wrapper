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

#include <thread>
#include <algorithm>

#include "../../host_selector/host_selector.h"
#include "../../host_selector/round_robin_host_selector.h"
#include "../../odbcapi_rds_helper.h"
#include "../../util/connection_string_keys.h"
#include "../../util/host_info_utils.h"
#include "../../util/map_utils.h"
#include "../../util/rds_utils.h"

AuroraInitialConnectionStrategyPlugin::AuroraInitialConnectionStrategyPlugin(DBC* dbc) : AuroraInitialConnectionStrategyPlugin(dbc, nullptr) {}

AuroraInitialConnectionStrategyPlugin::AuroraInitialConnectionStrategyPlugin(DBC* dbc, BasePlugin* next_plugin) : BasePlugin(dbc, next_plugin) {
    this->plugin_name = "INITIAL_CONNECTION";
    const std::map<std::string, std::string> conn_info = dbc->conn_attr;

    this->plugin_service_ = dbc->plugin_service;
    this->host_selector_ = plugin_service_->GetHostSelector();
    this->dialect_ = plugin_service_->GetDialect();

    this->retry_delay_ms_ = MapUtils::GetMillisecondsValue(conn_info, KEY_INITIAL_CONNECTION_RETRY_INTERVAL_MS, std::chrono::milliseconds(1000));
    this->retry_timeout_ms_ = MapUtils::GetMillisecondsValue(conn_info, KEY_INITIAL_CONNECTION_RETRY_TIMEOUT_MS, std::chrono::milliseconds(30000));
    this->verify_initial_connection_type_ = MapUtils::GetStringValue(conn_info, KEY_VERIFY_INITIAL_CONNECTION_TYPE, "");
    DLOG(INFO) << "verify connection type" << this->verify_initial_connection_type_;
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
        DLOG(INFO) << "Not an RDS Cluster DNS";
        return next_plugin->Connect(
        ConnectionHandle,
        WindowHandle,
        OutConnectionString,
        BufferLength,
        StringLengthPtr,
        DriverCompletion);
    }

    SQLRETURN rc;
    if (RdsUtils::IsRdsWriterClusterDns(host) || this->verify_initial_connection_type_ == "WRITER") {
        DLOG(INFO) << "WRITER";
        rc = this->GetVerifiedWriter(dbc, conn_info);
        if (rc == SQL_NULL_DATA) {
            return next_plugin->Connect(
            ConnectionHandle,
            WindowHandle,
            OutConnectionString,
            BufferLength,
            StringLengthPtr,
            DriverCompletion);
        }
        return rc;
    }

    if (RdsUtils::IsRdsReaderClusterDns(host) || this->verify_initial_connection_type_ == "READER") {
        DLOG(INFO) << "READER";
        rc = this->GetVerifiedReader(dbc, conn_info);
        if (rc == SQL_NULL_DATA) {
            return next_plugin->Connect(
            ConnectionHandle,
            WindowHandle,
            OutConnectionString,
            BufferLength,
            StringLengthPtr,
            DriverCompletion);
        }
        return rc;
    }

    return next_plugin->Connect(
        ConnectionHandle,
        WindowHandle,
        OutConnectionString,
        BufferLength,
        StringLengthPtr,
        DriverCompletion);
}

SQLRETURN AuroraInitialConnectionStrategyPlugin::GetVerifiedWriter(DBC* dbc, std::map<std::string, std::string> conn_info) {
    const std::chrono::time_point<std::chrono::steady_clock> end_time = std::chrono::steady_clock::now() + this->retry_timeout_ms_;
    while (std::chrono::steady_clock::now() < end_time) {
        SQLRETURN rc = SQL_ERROR;

        HostInfo writer_candidate = HostInfoUtils::GetWriter(this->plugin_service_->GetHosts());
        DLOG(INFO) << "WRITER CANDIDATE: " << writer_candidate.GetHost();
        if (writer_candidate.GetHost().empty() || RdsUtils::IsRdsClusterDns(writer_candidate.GetHost())) {
            DLOG(INFO) << "WRITER CANDIDATE empty";
            // connect with default
            rc = next_plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
            if (!SQL_SUCCEEDED(rc)) {
                plugin_service_->GetOdbcHelper()->Disconnect(dbc);
                // if network exception
                SQLSMALLINT stmt_length;
                SQLINTEGER native_error;
                SQLTCHAR sql_state[32] = { 0 };
                SQLTCHAR message[1024] = { 0 };
                RDS_SQLError(nullptr, static_cast<SQLHDBC>(dbc), nullptr, sql_state, &native_error, message, 1024, &stmt_length);
                if (this->dialect_->IsSqlStateNetworkError(AS_UTF8_CSTR(sql_state))) {
                    std::this_thread::sleep_for(retry_delay_ms_);
                    continue;
                }
                return rc;
            }
            this->plugin_service_->ForceRefreshHosts(false, 0);
            // identify new connection
            writer_candidate = plugin_service_->GetHostListProvider()->GetConnectionInfo(dbc);

            if (writer_candidate.GetHost().empty() || writer_candidate.GetHostRole() != WRITER) {
                plugin_service_->GetOdbcHelper()->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }

            // if isinitialconnection??
            plugin_service_->SetInitialHostInfo(writer_candidate);

            return rc;
        }

        dbc->conn_attr.insert_or_assign(KEY_SERVER, writer_candidate.GetHost());
        DLOG(INFO) << "WRITER CONNECT TRY";
        rc = next_plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

        if (!SQL_SUCCEEDED(rc)) {
            DLOG(INFO) << "WRITER CONNECT FAIL";
            plugin_service_->GetOdbcHelper()->Disconnect(dbc);
            // retry if network exception
            SQLSMALLINT stmt_length;
            SQLINTEGER native_error;
            SQLTCHAR sql_state[32] = { 0 };
            SQLTCHAR message[1024] = { 0 };
            RDS_SQLError(nullptr, static_cast<SQLHDBC>(dbc), nullptr, sql_state, &native_error, message, 1024, &stmt_length);
            if (this->dialect_->IsSqlStateNetworkError(AS_UTF8_CSTR(sql_state))) {
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }
        }
        DLOG(INFO) << "WRITER CONNECT SUCCESS";
        return rc;
    }

    return SQL_NULL_DATA; // TODO: is this the proper return?
}

SQLRETURN AuroraInitialConnectionStrategyPlugin::GetVerifiedReader(DBC* dbc, std::map<std::string, std::string> conn_info) {
    const std::chrono::time_point<std::chrono::steady_clock> end_time = std::chrono::steady_clock::now() + this->retry_timeout_ms_;

    // Get AWS Region
    std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_REGION, "");
    if (region.empty()) {
        region = dbc->conn_attr.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
            : "";
    }

    while (std::chrono::steady_clock::now() < end_time) {
        SQLRETURN rc = SQL_ERROR;

        HostInfo reader_candidate = this->GetReader(region);
        DLOG(INFO) << "READER CANDIDATE" << reader_candidate.GetHost();
        // If reader is empty or RDS Cluster endpoing
        if (reader_candidate.GetHost().empty() || RdsUtils::IsRdsClusterDns(reader_candidate.GetHost())) {
            // default connect
            rc = next_plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
            if (!SQL_SUCCEEDED(rc)) {
                plugin_service_->GetOdbcHelper()->Disconnect(dbc);
                // if network exception
                SQLSMALLINT stmt_length;
                SQLINTEGER native_error;
                SQLTCHAR sql_state[32] = { 0 };
                SQLTCHAR message[1024] = { 0 };
                RDS_SQLError(nullptr, static_cast<SQLHDBC>(dbc), nullptr, sql_state, &native_error, message, 1024, &stmt_length);
                if (this->dialect_->IsSqlStateNetworkError(AS_UTF8_CSTR(sql_state))) {
                    std::this_thread::sleep_for(retry_delay_ms_);
                    continue;
                }
                return rc;
            }
            // forcerefresh
            this->plugin_service_->ForceRefreshHosts(false, 0);
            // id new connection
            reader_candidate = plugin_service_->GetHostListProvider()->GetConnectionInfo(dbc);

            if (reader_candidate.GetHost().empty() || reader_candidate.GetHostRole() != READER) {
                plugin_service_->GetOdbcHelper()->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;

            }

            if (reader_candidate.GetHostRole() != READER) {
                if (this->HasNoReaders()) {
                    this->plugin_service_->SetInitialHostInfo(reader_candidate);
                    return rc;
                }
                plugin_service_->GetOdbcHelper()->Disconnect(dbc);
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }

            this->plugin_service_->SetInitialHostInfo(reader_candidate);
            return rc;
        }
        DLOG(INFO) << "SETTING READER CANDIDATE" << reader_candidate.GetHost();
        dbc->conn_attr.insert_or_assign(KEY_SERVER, reader_candidate.GetHost());
        rc = next_plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(rc)) {
            plugin_service_->GetOdbcHelper()->Disconnect(dbc);
            // if network or readonly exception
            SQLSMALLINT stmt_length;
            SQLINTEGER native_error;
            SQLTCHAR sql_state[32] = { 0 };
            SQLTCHAR message[1024] = { 0 };
            RDS_SQLError(nullptr, static_cast<SQLHDBC>(dbc), nullptr, sql_state, &native_error, message, 1024, &stmt_length);
            if (this->dialect_->IsSqlStateNetworkError(AS_UTF8_CSTR(sql_state))) {
                std::this_thread::sleep_for(retry_delay_ms_);
                continue;
            }
        }

        if (this->plugin_service_->GetHostListProvider()->GetConnectionRole(static_cast<SQLHDBC>(dbc)) != READER) {
            this->plugin_service_->ForceRefreshHosts(false, 0);
            if (this->HasNoReaders()) {
                this->plugin_service_->SetInitialHostInfo(reader_candidate);
                return rc;
            }
            plugin_service_->GetOdbcHelper()->Disconnect(dbc);
            std::this_thread::sleep_for(retry_delay_ms_);
            continue;
        }

        this->plugin_service_->SetInitialHostInfo(reader_candidate);
        return rc;
    }
    return {};
}

HostInfo AuroraInitialConnectionStrategyPlugin::GetReader(const std::string region) {

    std::vector<HostInfo> hosts = this->plugin_service_->GetHosts();
    std::vector<HostInfo> filtered_hosts;

    if (region.empty()) {
        filtered_hosts = hosts;
    } else {
        std::ranges::copy_if(hosts, std::back_inserter(filtered_hosts), [&](const HostInfo& host) {
            if (RdsUtils::GetRdsRegion(host.GetHost()) == region) {
                return true;
            }
            return false;
        });
    }
    std::unordered_map<std::string, std::string> properties;
    RoundRobinHostSelector::SetRoundRobinWeight(filtered_hosts, properties);
    return this->host_selector_->GetHost(filtered_hosts, false, properties);
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
        return true;
    }

    // Went through all hosts and found no reader.
    return true;
}
