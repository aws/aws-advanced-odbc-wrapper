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

#include "failover_plugin.h"

#include "../base_plugin.h"
#include "../../odbcapi.h"
#include "../../odbcapi_rds_helper.h"

#include "../../util/connection_string_helper.h"
#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/rds_lib_loader.h"
#include "../../util/rds_utils.h"
#include "../../util/init_plugin_helper.h"
#include "../../util/odbc_helper.h"

#include "../../host_selector/highest_weight_host_selector.h"
#include "../../host_selector/host_selector.h"
#include "../../host_selector/random_host_selector.h"
#include "../../host_selector/round_robin_host_selector.h"

FailoverPlugin::FailoverPlugin(DBC *dbc) : FailoverPlugin(dbc, nullptr) {}

FailoverPlugin::FailoverPlugin(DBC *dbc, BasePlugin *next_plugin) : FailoverPlugin(dbc, next_plugin, FailoverMode::UNKNOWN_FAILOVER_MODE, nullptr, nullptr, nullptr, nullptr) {}

FailoverPlugin::FailoverPlugin(DBC *dbc, BasePlugin *next_plugin, FailoverMode failover_mode, std::shared_ptr<Dialect> dialect, std::shared_ptr<HostSelector> host_selector, std::shared_ptr<ClusterTopologyQueryHelper> topology_query_helper, std::shared_ptr<ClusterTopologyMonitor> topology_monitor) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "FAILOVER";
    std::map<RDS_STR, RDS_STR> conn_info = dbc->conn_attr;

    this->failover_timeout_ms_= conn_info.contains(KEY_FAILOVER_TIMEOUT) ?
        std::chrono::milliseconds(std::strtol(conn_info.at(KEY_FAILOVER_TIMEOUT).c_str(), nullptr, 10))
        : DEFAULT_FAILOVER_TIMEOUT_MS;
    this->cluster_id_ = InitClusterId(conn_info);
    this->failover_mode_ = failover_mode != FailoverMode::UNKNOWN_FAILOVER_MODE ? failover_mode : InitFailoverMode(conn_info);
    this->dialect_ = dialect ? dialect : InitDialect(conn_info);
    this->host_selector_ = host_selector ? host_selector : InitHostSelectorStrategy(conn_info);
    this->topology_query_helper_ = topology_query_helper ? topology_query_helper : InitQueryHelper(dbc);
    this->topology_monitor_ = topology_monitor ? topology_monitor : InitTopologyMonitor(dbc);
}

FailoverPlugin::~FailoverPlugin()
{
    topology_monitor_.reset();
}

SQLRETURN FailoverPlugin::Connect(
    SQLHDBC ConnectionHandle,
    SQLHWND WindowHandle,
    SQLTCHAR *OutConnectionString,
    SQLSMALLINT BufferLength,
    SQLSMALLINT *StringLengthPtr,
    SQLUSMALLINT DriverCompletion)
{
    DBC* dbc = (DBC*) ConnectionHandle;
    topology_monitor_->StartMonitor(dbc->plugin_head);
    return next_plugin->Connect(
        ConnectionHandle,
        WindowHandle,
        OutConnectionString,
        BufferLength,
        StringLengthPtr,
        DriverCompletion
    );
}

SQLRETURN FailoverPlugin::Execute(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    STMT* stmt = (STMT*) StatementHandle;
    DBC* dbc = stmt->dbc;
    SQLRETURN ret = next_plugin->Execute(StatementHandle, StatementText, TextLength);

    if (SQL_SUCCEEDED(ret)) {
        return ret;
    }

    SQLSMALLINT stmt_length;
    SQLINTEGER native_error;
    SQLTCHAR sql_state[MAX_STATE_LENGTH] = { 0 }, message[MAX_MSG_LENGTH] = { 0 };
    RDS_SQLError(nullptr, nullptr, stmt, sql_state, &native_error, message, MAX_MSG_LENGTH, &stmt_length);
    if (!CheckShouldFailover(AS_RDS_CHAR(sql_state))) {
        return ret;
    }

    // Invalidate statements, but don't fully clean up
    for (STMT* stmt : dbc->stmt_list) {
        stmt->wrapped_stmt = nullptr;
        delete stmt->err;
        stmt->err = new ERR_INFO("Failed to switch to a new connection.", ERR_FAILOVER_FAILED);;
    }
    // and descriptors
    for (DESC* desc : dbc->desc_list) {
        desc->wrapped_desc = nullptr;
        delete desc->err;
        desc->err = new ERR_INFO("Failed to switch to a new connection.", ERR_FAILOVER_FAILED);
    }

    bool failover_result = false;
    TRANSACTION_STATUS original_transaction_status = dbc->transaction_status;
    if (failover_mode_ == STRICT_WRITER) {
        failover_result = FailoverWriter(dbc);
    } else {
        failover_result = FailoverReader(dbc);
    }

    if (failover_result) {
        ERR_INFO* err_info;
        if (TRANSACTION_OPEN == original_transaction_status) {
            dbc->transaction_status = TRANSACTION_CLOSED;
            err_info = new ERR_INFO("Transaction resolution unknown. Please re-configure session state if required and try restarting the transaction.", ERR_FAILOVER_UNKNOWN_TRANSACTION_STATE);
        } else {
            err_info = new ERR_INFO("The active connection has changed due to a connection failure. Please re-configure session state if required.", ERR_FAILOVER_SUCCESS);
        }
        // Set failover error messages for all related statements
        for (STMT* stmt : dbc->stmt_list) {
            delete stmt->err;
            stmt->err = new ERR_INFO(*err_info);
        }
        // and descriptors
        for (DESC* desc : dbc->desc_list) {
            delete desc->err;
            stmt->err = new ERR_INFO(*err_info);
        }
    }

    return ret;
}

bool FailoverPlugin::CheckShouldFailover(RDS_CHAR* sql_state)
{
    // Check if the SQL State is related to a communication error
    bool should_failover = this->dialect_->IsSqlStateNetworkError(sql_state);
    return should_failover;
}

void FailoverPlugin::RemoveHostCandidate(const std::string &host, std::vector<HostInfo> &candidates)
{
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
        [host](HostInfo const& h) { return h.GetHost() == host; }),
        candidates.end());
}

bool FailoverPlugin::FailoverReader(DBC *dbc)
{
    auto curr_time = std::chrono::steady_clock::now();
    auto end = curr_time + failover_timeout_ms_;

    LOG(INFO) << "Starting reader failover procedure.";
    // When we pass a timeout of 0, we inform the plugin service that it should update its topology without waiting
    // for it to get updated, since we do not need updated topology to establish a reader connection.
    topology_monitor_->ForceRefresh(false, 0);

    // The roles in this list might not be accurate, depending on whether the new topology has become available yet.
    std::vector<HostInfo> hosts = topology_map_->Get(cluster_id_);
    if (hosts.empty()) {
        LOG(INFO) << "No topology available.";
        return false;
    }

    std::vector<HostInfo> reader_candidates;
    HostInfo original_writer;

    for (const auto& host : hosts) {
        if (host.IsHostWriter()) {
            original_writer = host;
        } else {
            reader_candidates.push_back(host);
        }
    }

    std::unordered_map<std::string, std::string> properties;
    if (ROUND_ROBIN == host_selector_strategy_) {
        RoundRobinHostSelector::SetRoundRobinWeight(reader_candidates, properties);
    }

    std::string host_string;
    bool is_original_writer_still_writer = false;
    do {
        std::vector<HostInfo> remaining_readers(reader_candidates);
        while (!remaining_readers.empty() && (curr_time = std::chrono::steady_clock::now()) < end) {
            LOG(INFO) << "Failover for ClusterId: " << cluster_id_ << ". Remaining Hosts: " << remaining_readers.size();
            HostInfo host;
            try {
                host = host_selector_->GetHost(remaining_readers, false, properties);
                host_string = host.GetHost();
                LOG(INFO) << "[Failover Service] Selected Host: " << host_string;
            } catch (const std::exception& e) {
                LOG(INFO) << "[Failover Service] no hosts in topology for: " << cluster_id_;
                return false;
            }
            bool is_connected = ConnectToHost(dbc, host_string);
            if (!is_connected) {
                LOG(INFO) << "[Failover Service] unable to connect to: " << host_string;
                RemoveHostCandidate(host_string, remaining_readers);
                continue;
            }

            if (!GetNodeId(dbc, dialect_).empty()) {
                bool is_reader = topology_query_helper_->GetWriterId(dbc).empty();
                if (is_reader || (this->failover_mode_ != STRICT_READER)) {
                    LOG(INFO) << "[Failover Service] connected to a new reader for: " << host_string;
                    curr_host_ = host;
                    return true;
                }
                LOG(INFO) << "[Failover Service] Strict Reader Mode, not connected to a reader: " << host_string;
            }
            RemoveHostCandidate(host_string, remaining_readers);
            NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
                dbc->wrapped_dbc
            );
            LOG(INFO) << "[Failover Service] Cleaned up first connection, required a strict reader: " << host_string << ", " << dbc;

            if (host.IsHostWriter()) {
                // The reader candidate is actually a writer, which is not valid when failoverMode is STRICT_READER.
                // We will remove it from the list of reader candidates to avoid retrying it in future iterations.
                RemoveHostCandidate(host_string, reader_candidates);
            }
        }

        // We were not able to connect to any of the original readers. We will try connecting to the original writer,
        // which may have been demoted to a reader.
        if (std::chrono::steady_clock::now() > end) {
            // Timed out.
            continue;
        }

        if (this->failover_mode_ == STRICT_READER && is_original_writer_still_writer) {
            // The original writer has been verified, so it is not valid when in STRICT_READER mode.
            continue;
        }

        // Try the original writer, which may have been demoted to a reader.
        host_string = original_writer.GetHost();
        bool is_connected = ConnectToHost(dbc, host_string);
        if (is_connected) {
            if (GetNodeId(dbc, dialect_).empty()) {
                NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
                    dbc->wrapped_dbc
                );
                continue;
            }
            if (!topology_query_helper_->GetWriterId(dbc).empty()) {
                is_original_writer_still_writer = true;
                if (STRICT_READER == this->failover_mode_) {
                    LOG(INFO) << "[Failover Service] Strict Reader Mode, not connected to a reader: " << host_string;
                    continue;
                }
            }
            LOG(INFO) << "[Failover Service] reader failover connected to writer instance for: " << host_string;
            curr_host_ = original_writer;
            return true;
        } else {
            LOG(INFO) << "[Failover Service] Failed to connect to host: " << original_writer;
        }

    } while (std::chrono::steady_clock::now() < end);

    // Timed out.
    NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
        dbc->wrapped_dbc
    );
    LOG(INFO) << "[Failover Service] The reader failover process was not able to establish a connection before timing out.";
    return false;
}

bool FailoverPlugin::FailoverWriter(DBC *dbc)
{
    topology_monitor_->ForceRefresh(true, failover_timeout_ms_.count());

    // Try connecting to a writer
    std::vector<HostInfo> hosts = topology_map_->Get(cluster_id_);
    std::unordered_map<std::string, std::string> properties;
    RoundRobinHostSelector::SetRoundRobinWeight(hosts, properties);
    HostInfo host;
    try {
        host = host_selector_->GetHost(hosts, true, properties);
    } catch (const std::exception& e) {
        LOG(INFO) << "[Failover Service] no hosts in topology for: " << cluster_id_;
        return false;
    }
    std::string host_string = host.GetHost();
    LOG(INFO) << "[Failover Service] writer failover connection to a new writer: " << host_string;

    bool is_connected = ConnectToHost(dbc, host_string);
    if (!is_connected) {
        LOG(INFO) << "[Failover Service] writer failover unable to connect to any instance for: " << cluster_id_;
        return false;
    }
    if (!GetNodeId(dbc, dialect_).empty()) {
        if (!topology_query_helper_->GetWriterId(dbc).empty()) {
            LOG(INFO) << "[Failover Service] writer failover connected to a new writer for: " << host_string;
            curr_host_ = host;
            return true;
        }
        LOG(ERROR) << "The new writer was identified to be " << host_string << ", but querying the instance for its role returned a reader.";
        return false;
    }
    LOG(INFO) << "[Failover Service] writer failover unable to connect to any instance for: " << cluster_id_;
    NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
        dbc->wrapped_dbc
    );
    return false;
}

bool FailoverPlugin::ConnectToHost(DBC *dbc, const std::string &host_string)
{
    if (TRANSACTION_OPEN == dbc->transaction_status) {
        // TODO - Need to revisit rolling back internally
        //  Using psqlodbc's rollback causes issues with the underlying driver
        //  where it would encounter a read access violation
        //  potentially a order of operation issue
        // Set transaction to closed internally
        // we are cleaning up the underlying DBC
        dbc->transaction_status = TRANSACTION_CLOSED;
    }

    NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
        dbc->wrapped_dbc
    );
    LOG(INFO) << "Attempting to connect to host: " << host_string;
    dbc->conn_attr.insert_or_assign(KEY_SERVER, host_string);

    return SQL_SUCCEEDED(dbc->plugin_head->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));
}

std::string FailoverPlugin::InitClusterId(std::map<RDS_STR, RDS_STR> conn_info)
{
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

FailoverMode FailoverPlugin::InitFailoverMode(std::map<RDS_STR, RDS_STR> conn_info)
{
    FailoverMode mode = UNKNOWN_FAILOVER_MODE;
    if (conn_info.contains(KEY_FAILOVER_MODE)) {
        RDS_STR local_str = conn_info.at(KEY_FAILOVER_MODE);
        std::string local_str_upper = RDS_STR_UPPER(local_str);
        if (failover_mode_table.contains(local_str_upper)) {
            mode = failover_mode_table.at(local_str_upper);
        }
    }

    if (failover_mode_ == UNKNOWN_FAILOVER_MODE) {
        std::string host = conn_info.contains(KEY_SERVER) ? conn_info.at(KEY_SERVER) : "";
        mode = RdsUtils::IsRdsReaderClusterDns(host) ? READER_OR_WRITER : STRICT_WRITER;
    }

    return mode;
}

std::shared_ptr<HostSelector> FailoverPlugin::InitHostSelectorStrategy(std::map<RDS_STR, RDS_STR> conn_info)
{
    host_selector_strategy_ = RANDOM_HOST;
    if (conn_info.contains(KEY_HOST_SELECTOR_STRATEGY)) {
        host_selector_strategy_ = HostSelector::GetHostSelectorStrategy(conn_info.at(KEY_HOST_SELECTOR_STRATEGY));
    }

    switch (host_selector_strategy_) {
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

std::shared_ptr<ClusterTopologyQueryHelper> FailoverPlugin::InitQueryHelper(DBC *dbc)
{
    std::map<RDS_STR, RDS_STR> conn_info = dbc->conn_attr;

    std::string endpoint_template = conn_info.contains(KEY_ENDPOINT_TEMPLATE) ? conn_info.at(KEY_ENDPOINT_TEMPLATE) : "";
    std::string host = conn_info.contains(KEY_SERVER) ? conn_info.at(KEY_SERVER) : "";
    if (endpoint_template.empty()) {
        endpoint_template = RdsUtils::GetRdsInstanceHostPattern(host);
    }

    int port = conn_info.contains(KEY_PORT) ?
        std::strtol(conn_info.at(KEY_PORT).c_str(), nullptr, 10) :
        dialect_->GetDefaultPort();

    return std::make_shared<ClusterTopologyQueryHelper>(
        dbc->env->driver_lib_loader, port, endpoint_template,
        dialect_->GetTopologyQuery(), dialect_->GetWriterIdQuery(), dialect_->GetNodeIdQuery()
    );
}

std::shared_ptr<ClusterTopologyMonitor> FailoverPlugin::InitTopologyMonitor(DBC *dbc)
{
    std::shared_ptr<ClusterTopologyMonitor> topology_monitor = std::make_shared<ClusterTopologyMonitor>(
        dbc, topology_map_, topology_query_helper_, dialect_
    );
    return topology_monitor;
}
