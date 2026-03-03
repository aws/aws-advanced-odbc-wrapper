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

#include "../../util/plugin_service.h"
#include "../../util/utils.h"

ReadWriteSplittingPlugin::ReadWriteSplittingPlugin(DBC *dbc) : ReadWriteSplittingPlugin(dbc, nullptr) {}

ReadWriteSplittingPlugin::ReadWriteSplittingPlugin(DBC *dbc, BasePlugin *next_plugin) : AbstractReadWriteSplittingPlugin(dbc, next_plugin) {
    this->host_selector_ = plugin_service_->GetHostSelector();
}

void ReadWriteSplittingPlugin::RefreshAndStoreTopology() {
    if (IsConnectionUsable(current_connection_)) {
        this->plugin_service_->RefreshHosts();
    }

    this->hosts_ = this->plugin_service_->GetHosts();
    if (this->hosts_.empty()) {
        LOG(INFO) << "Host list is empty.";
        throw std::runtime_error("Host list is empty.");
    }
    this->writer_host_info_ = Utils::GetWriter(this->hosts_);
}

SQLRETURN ReadWriteSplittingPlugin::InitializeWriterConnection() {
    SQLHDBC local_hdbc;
    // Open a new connection
    this->odbc_helper_->AllocDbc(henv_, local_hdbc);
    DBC *conn = static_cast<DBC*>(local_hdbc);
    conn->conn_attr = connection_attributes_;
    std::unordered_map<std::string, std::string> properties;
    HostInfo host_info = this->host_selector_->GetHost(this->plugin_service_->GetHosts(), true, properties);
    conn->conn_attr.insert_or_assign(KEY_SERVER, host_info.GetHost());
    SQLRETURN ret = plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_helper_->DisconnectAndFree(&local_hdbc);
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
        LOG(WARNING) << "A reader instance was requested, but there are no readers in the host list. The current writer will be used as a fallback: '" + writer_host_info_.GetHost() +"'";
        if (!IsConnectionUsable(this->writer_connection_)) {
            return InitializeWriterConnection();
        }
    } else {
        LOG(INFO) << "Attempting to switch from a writer to reader host '" + reader_host_info_.GetHost() + "'";
        return OpenNewReaderConnection();
    }
    return SQL_ERROR;
}

SQLRETURN ReadWriteSplittingPlugin::OpenNewReaderConnection() {
    DBC* conn = nullptr;
    std::vector<HostInfo> host_candidates = this->plugin_service_->GetHosts();
    int conn_attempts = host_candidates.size() * 2;
    std::unordered_map<std::string, std::string> properties;
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
            LOG(INFO) << "Failed to connect to reader host: '" + host_info.GetHost() + "'";
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
        LOG(INFO) << "The plugin was unable to establish a reader connection to any reader instance.";
        return SQL_ERROR;
    }

    LOG(INFO) << "Successfully connected to a new reader host: '" + host_info.GetHost() + "'";
    SetReaderConnection(conn, host_info);
    SwitchCurrentConnectionTo(conn, host_info);
    return ret;
}

void ReadWriteSplittingPlugin::CloseReaderIfNecessary(SQLHDBC current_conn) {
    if (!reader_host_info_.GetHost().empty()) {
        CloseReaderConnectionIfIdle();
    }
}

bool ReadWriteSplittingPlugin::ShouldUpdateWriterConnection(HostInfo current_host) {
    return current_host.GetHostRole() == WRITER;
}

bool ReadWriteSplittingPlugin::ShouldUpdateReaderConnection(HostInfo current_host) {
    return current_host.GetHostRole() == READER;
}
