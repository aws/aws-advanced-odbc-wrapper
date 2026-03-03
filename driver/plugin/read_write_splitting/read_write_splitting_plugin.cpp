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

#include "read_write_splitting_plugin.h"

#include "../../host_selector/highest_weight_host_selector.h"
#include "../../host_selector/round_robin_host_selector.h"
#include "../../host_selector/random_host_selector.h"
#include "../../util/map_utils.h"
#include "../../util/plugin_service.h"

ReadWriteSplittingPlugin::ReadWriteSplittingPlugin(DBC *dbc) : ReadWriteSplittingPlugin(dbc, nullptr) {}

ReadWriteSplittingPlugin::ReadWriteSplittingPlugin(DBC *dbc, BasePlugin *next_plugin) : AbstractReadWriteSplittingPlugin(dbc, next_plugin) {
    this->host_selector_ = InitRwHostSelector(dbc->conn_attr);
}

std::shared_ptr<HostSelector> ReadWriteSplittingPlugin::InitRwHostSelector(
    const std::map<std::string, std::string>& conn_info) {

    HostSelectorStrategies selector_strategy = RANDOM_HOST;
    if (conn_info.contains(KEY_RW_HOST_SELECTOR_STRATEGY)) {
        selector_strategy = HostSelector::GetHostSelectorStrategy(MapUtils::GetStringValue(conn_info, KEY_RW_HOST_SELECTOR_STRATEGY, VALUE_RANDOM_HOST_SELECTOR));
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

SQLRETURN ReadWriteSplittingPlugin::RefreshAndStoreTopology() {
    if (IsConnectionUsable(current_connection_)) {
        plugin_service_->RefreshHosts();
    }

    this->hosts_ = plugin_service_->GetHosts();
    if (this->hosts_.empty()) {
        SetStmtError("Host list is empty.", ERR_RW_SWITCH_FAILED);
        return SQL_ERROR;
    }
    this->writer_host_info_ = plugin_service_->GetTopologyUtil()->GetWriter(this->hosts_);
    return SQL_SUCCESS;
}

SQLRETURN ReadWriteSplittingPlugin::InitializeWriterConnection() {
    SQLHDBC local_hdbc;
    // Open a new connection
    this->odbc_helper_->AllocDbc(henv_, local_hdbc);
    DBC *conn = static_cast<DBC*>(local_hdbc);
    conn->conn_attr = connection_attributes_;
    const std::unordered_map<std::string, std::string> properties;
    const HostInfo host_info = this->host_selector_->GetHost(plugin_service_->GetHosts(), true, properties);
    conn->conn_attr.insert_or_assign(KEY_SERVER, host_info.GetHost());
    const SQLRETURN ret = plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_helper_->DisconnectAndFree(&local_hdbc);
        SetStmtError("The plugin was unable to establish a writer connection.", ERR_RW_WRITER_SWITCH_FAILED);
        return ret;
    }
    {
        const std::lock_guard<std::recursive_mutex> lock_guard(dbc_->env->lock);
        dbc_->env->dbc_list.remove(conn);
    }

    SetWriterConnection(conn, host_info);
    SwitchCurrentConnectionTo(conn, host_info);
    return ret;
}

SQLRETURN ReadWriteSplittingPlugin::InitializeReaderConnection() {
    if (this->hosts_.size() == 1) {
        LOG(WARNING) << "A reader instance was requested, but there are no readers in the host list. The current writer will be used as a fallback: '" << writer_host_info_.GetHost() << "'";
        if (this->writer_connection_ && !IsConnectionUsable(this->writer_connection_->wrapped_dbc)) {
            return InitializeWriterConnection();
        }
        return SQL_SUCCESS;
    }

    LOG(INFO) << "Attempting to switch from a writer to reader host '" << reader_host_info_.GetHost() << "'";
    return OpenNewReaderConnection();
}

SQLRETURN ReadWriteSplittingPlugin::OpenNewReaderConnection() {
    DBC* conn = nullptr;
    const std::vector<HostInfo> host_candidates = plugin_service_->GetHosts();
    const size_t conn_attempts = host_candidates.size() * 2;
    const std::unordered_map<std::string, std::string> properties;
    SQLRETURN ret = SQL_ERROR;
    HostInfo host_info;

    for (int i = 0; i < conn_attempts; i++) {
        host_info = this->host_selector_->GetHost(host_candidates, false, properties);
        SQLHDBC local_hdbc = SQL_NULL_HANDLE;
        // Open a new connection
        this->odbc_helper_->AllocDbc(henv_, local_hdbc);
        conn = static_cast<DBC*>(local_hdbc);
        conn->conn_attr = connection_attributes_;
        conn->conn_attr.insert_or_assign(KEY_SERVER, host_info.GetHost());
        ret = plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(ret)) {
            LOG(INFO) << "Failed to connect to reader host: '" << host_info.GetHost() << "'";
            odbc_helper_->DisconnectAndFree(&local_hdbc);
        } else {
            {
                const std::lock_guard<std::recursive_mutex> lock_guard(dbc_->env->lock);
                dbc_->env->dbc_list.remove(conn);
            }
            break;
        }
    }

    if (conn == nullptr || !SQL_SUCCEEDED(ret)) {
        SetStmtError("The plugin was unable to establish a reader connection to any reader instance.", ERR_RW_READER_SWITCH_FAILED);
        return SQL_ERROR;
    }

    LOG(INFO) << "Successfully connected to a new reader host: '" << host_info.GetHost() << "'";
    SetReaderConnection(conn, host_info);
    SwitchCurrentConnectionTo(conn, host_info);
    return ret;
}

void ReadWriteSplittingPlugin::CloseReaderIfNecessary() {
    if (!reader_host_info_.GetHost().empty()) {
        CloseReaderConnectionIfIdle();
    }
}

bool ReadWriteSplittingPlugin::ShouldUpdateWriterConnection(const HostInfo &current_host) {
    return current_host.GetHostRole() == WRITER;
}

bool ReadWriteSplittingPlugin::ShouldUpdateReaderConnection(const HostInfo &current_host) {
    return current_host.GetHostRole() == READER;
}
