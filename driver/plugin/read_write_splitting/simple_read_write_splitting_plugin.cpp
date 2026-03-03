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

#include "simple_read_write_splitting_plugin.h"

#include "../../util/plugin_service.h"
#include "../../util/map_utils.h"
#include "../../util/rds_utils.h"

SimpleReadWriteSplittingPlugin::SimpleReadWriteSplittingPlugin(DBC *dbc) : SimpleReadWriteSplittingPlugin(dbc, nullptr) {}

SimpleReadWriteSplittingPlugin::SimpleReadWriteSplittingPlugin(DBC *dbc, BasePlugin *next_plugin) : AbstractReadWriteSplittingPlugin(dbc, next_plugin) {
    if (!dbc->conn_attr.contains(KEY_SRW_READ_ENDPOINT) || !dbc->conn_attr.contains(KEY_SRW_WRITE_ENDPOINT)) {
        throw std::runtime_error("Please ensure the SRW_WRITE_ENDPOINT and SRW_READ_ENDPOINT parameters have been set.");
    }
    this->read_endpoint = dbc->conn_attr.at(KEY_SRW_READ_ENDPOINT);
    this->write_endpoint = dbc->conn_attr.at(KEY_SRW_WRITE_ENDPOINT);
    this->verify_new_conns_ = dbc->conn_attr.contains(KEY_SRW_VERIFY_CONNS) && dbc->conn_attr.at(KEY_SRW_VERIFY_CONNS) == VALUE_BOOL_TRUE;
    this->verify_initial_conn_type_ = UNKNOWN;
    if (dbc->conn_attr.contains(KEY_SRW_VERIFY_INITIAL_CONN_TYPE)) {
        if (dbc->conn_attr.at(KEY_SRW_VERIFY_INITIAL_CONN_TYPE) == "WRITER") {
            this->verify_initial_conn_type_ = WRITER;
        } else if (dbc->conn_attr.at(KEY_SRW_VERIFY_INITIAL_CONN_TYPE) == "READER") {
            this->verify_initial_conn_type_ = READER;
        }
    }
    this->connect_retry_timeout_ms = MapUtils::GetMillisecondsValue(dbc->conn_attr, KEY_SRW_CONN_TIMEOUT_MS, std::chrono::milliseconds(60));
    this->connect_retry_interval_ms = MapUtils::GetMillisecondsValue(dbc->conn_attr, KEY_SRW_CONN_INTERVAL_MS, std::chrono::milliseconds(60));
}

SQLRETURN SimpleReadWriteSplittingPlugin::Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) {
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    bool skip_plugin = MapUtils::GetBooleanValue(dbc->conn_attr, INTERNAL_KEY_SRW_SKIP, false);
    if (!this->verify_new_conns_ || skip_plugin) {
        // No verification required. Continue with a normal workflow.
        return next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    std::string host = MapUtils::GetStringValue(dbc_->conn_attr, KEY_SERVER, "");

    DBC* conn = SQL_NULL_HDBC;
    if (RdsUtils::IsRdsWriterClusterDns(host) || this->verify_initial_conn_type_ == WRITER) {
        conn = GetVerifiedConnection(host, WRITER, ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    } else if (RdsUtils::IsRdsReaderClusterDns(host) || this->verify_initial_conn_type_ == READER) {
        conn = GetVerifiedConnection(host, READER, ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    SQLRETURN ret = SQL_SUCCESS;
    if (!conn) {
        // Continue with a normal workflow.
        ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    return ret;
}

void SimpleReadWriteSplittingPlugin::SetInitialConnectionHostInfo(SQLHDBC conn, HostInfo host_info) {
    if (host_info.GetHost().empty()) {
        this->plugin_service_->ForceRefreshHosts(false, 0);
        host_info = this->plugin_service_->GetHostListProvider()->GetConnectionInfo(conn);
    }

    if (host_info.GetHost().empty()) {
        this->plugin_service_->SetInitialHostInfo(host_info);
    }
}

DBC *SimpleReadWriteSplittingPlugin::GetVerifiedConnection(
    std::string host,
    HOST_ROLE role,
    SQLHDBC ConnectionHandle,
    SQLHWND WindowHandle,
    SQLTCHAR *OutConnectionString,
    SQLSMALLINT BufferLength,
    SQLSMALLINT *StringLengthPtr,
    SQLUSMALLINT DriverCompletion)
{
    DBC* conn = SQL_NULL_HDBC;
    SQLHDBC local_hdbc = SQL_NULL_HANDLE;
    SQLRETURN ret = SQL_ERROR;

    std::chrono::time_point end_time = std::chrono::steady_clock::now() + this->connect_retry_timeout_ms;
    while (std::chrono::steady_clock::now() < end_time) {
        if (ConnectionHandle != SQL_NULL_HDBC) {
            ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
            if (SQL_SUCCEEDED(ret)) {
                conn = static_cast<DBC*>(ConnectionHandle);
            }
        } else if (!host.empty()) {
            this->odbc_helper_->AllocDbc(henv_, local_hdbc);
            conn = static_cast<DBC*>(local_hdbc);
            conn->conn_attr = connection_attributes_;
            conn->conn_attr.insert_or_assign(INTERNAL_KEY_SRW_SKIP, VALUE_BOOL_TRUE); // Skip this plugin.
            conn->conn_attr.insert_or_assign(KEY_SERVER, host);
            ret = plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
            if (!SQL_SUCCEEDED(ret)) {
                this->odbc_helper_->DisconnectAndFree(&local_hdbc);
            } else {
                conn->conn_attr.erase(INTERNAL_KEY_SRW_SKIP);
                {
                    const std::lock_guard<std::recursive_mutex> lock_guard(dbc_->env->lock);
                    dbc_->env->dbc_list.remove(conn);
                }
            }
        } else {
            conn = SQL_NULL_HDBC;
            local_hdbc = SQL_NULL_HANDLE;
            break;
        }

        if (conn == SQL_NULL_HDBC || !SQL_SUCCEEDED(ret) || (conn && this->plugin_service_->GetHostListProvider()->GetConnectionRole(conn) != role)) {
            // The connection does not have the desired role. Retry.
            conn = SQL_NULL_HDBC;
            local_hdbc = SQL_NULL_HANDLE;
            std::this_thread::sleep_for(std::chrono::milliseconds(this->connect_retry_interval_ms));
            continue;
        }

        // Connection is valid and verified.
        return conn;
    }

    LOG(ERROR) << "The plugin was unable to establish a " << (role == READER ? "read only" : "read write") << " connection within " << this->connect_retry_timeout_ms << " ms.";
    return conn;
}

SQLRETURN SimpleReadWriteSplittingPlugin::InitializeReaderConnection() {
    if (this->reader_host_info_.GetHost().empty()) {
        this->reader_host_info_ = CreateHostInfo(this->read_endpoint, READER);
    }

    DBC* conn = SQL_NULL_HDBC;
    SQLHDBC local_hdbc = SQL_NULL_HDBC;
    SQLRETURN ret = SQL_ERROR;

    if (this->verify_new_conns_) {
        conn = GetVerifiedConnection(this->read_endpoint, READER, nullptr, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    } else {
        this->odbc_helper_->AllocDbc(henv_, local_hdbc);
        conn = static_cast<DBC*>(local_hdbc);
        conn->conn_attr = connection_attributes_;
        conn->conn_attr.insert_or_assign(KEY_SERVER, this->read_endpoint);
        conn->conn_attr.insert_or_assign(INTERNAL_KEY_SRW_SKIP, VALUE_BOOL_TRUE); // Skip this plugin.
        ret = plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(ret)) {
            odbc_helper_->DisconnectAndFree(&local_hdbc);
        } else {
            {
                const std::lock_guard<std::recursive_mutex> lock_guard(dbc_->env->lock);
                dbc_->env->dbc_list.remove(conn);
            }
            HostInfo temp = plugin_service_->GetHostListProvider()->GetConnectionInfo(conn);
            conn->conn_attr.erase(INTERNAL_KEY_SRW_SKIP);
        }
    }
    if (conn == SQL_NULL_HDBC) {
        LOG(ERROR) << "Failed to connect to reader host: '" << this->reader_host_info_.GetHostPortPair() << "'";
        throw std::runtime_error("Failed to connect to reader host: '" + this->reader_host_info_.GetHostPortPair() + "'");
    }

    LOG(INFO) << "Successfully connected to a new reader host: '" + this->reader_host_info_.GetHostPortPair() + "'";
    SetReaderConnection(conn, reader_host_info_);
    SwitchCurrentConnectionTo(conn, reader_host_info_);
    LOG(INFO) << "Switched from a writer to a reader host. New reader host: '" << this->read_endpoint + "'";
    return SQL_SUCCESS;
}

SQLRETURN SimpleReadWriteSplittingPlugin::InitializeWriterConnection() {
    if (this->writer_host_info_.GetHost().empty()) {
        this->writer_host_info_ = CreateHostInfo(this->write_endpoint, WRITER);
    }

    DBC* conn = SQL_NULL_HDBC;
    SQLHDBC local_hdbc = SQL_NULL_HDBC;
    SQLRETURN ret = SQL_ERROR;

    if (this->verify_new_conns_) {
        conn = GetVerifiedConnection(this->write_endpoint, WRITER, nullptr, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    } else {
        this->odbc_helper_->AllocDbc(henv_, local_hdbc);
        conn = static_cast<DBC*>(local_hdbc);
        conn->conn_attr = connection_attributes_;
        conn->conn_attr.insert_or_assign(KEY_SERVER, this->write_endpoint);
        conn->conn_attr.insert_or_assign(INTERNAL_KEY_SRW_SKIP, VALUE_BOOL_TRUE); // Skip this plugin.
        ret = plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(ret)) {
            odbc_helper_->DisconnectAndFree(&local_hdbc);
        } else {
            conn->conn_attr.erase(INTERNAL_KEY_SRW_SKIP);
            {
                const std::lock_guard<std::recursive_mutex> lock_guard(dbc_->env->lock);
                dbc_->env->dbc_list.remove(conn);
            }
        }
    }

    if (conn == SQL_NULL_HDBC) {
        LOG(ERROR) << "A writer connection was requested, but the plugin was unable to establish a writer connection with the writer endpoint '" << this->write_endpoint << "'.";
        throw std::runtime_error("A writer connection was requested, but the plugin was unable to establish a writer connection with the writer endpoint '" + this->write_endpoint + "'.");
    }

    SetWriterConnection(conn, this->writer_host_info_);
    SwitchCurrentConnectionTo(conn, this->writer_host_info_);
    return SQL_SUCCESS;
}

HostInfo SimpleReadWriteSplittingPlugin::CreateHostInfo(const std::string &endpoint, HOST_ROLE role) {
    int port = this->plugin_service_->GetDialect()->GetDefaultPort();
    if (dbc_->conn_attr.contains(KEY_PORT)) {
        port = MapUtils::GetIntValue(dbc_->conn_attr, KEY_PORT, port);
    }

    return HostInfo(endpoint, port, UP, role);
}

void SimpleReadWriteSplittingPlugin::RefreshAndStoreTopology() {
    // Simple Read/Write Splitting does not rely on topology.
}

void SimpleReadWriteSplittingPlugin::CloseReaderIfNecessary(SQLHDBC current_conn) {
    // Simple Read/Write will connect to the reader endpoint regardless.
}

bool SimpleReadWriteSplittingPlugin::ShouldUpdateReaderConnection(HostInfo current_host) {
    DBC* cached_reader_conn = this->reader_cache_item_.Get();
    return current_host.GetHostRole() == READER && cached_reader_conn->wrapped_dbc != current_connection_ && (!this->verify_new_conns_ || this->plugin_service_->GetHostListProvider()->GetConnectionRole(current_connection_) == READER);
}

bool SimpleReadWriteSplittingPlugin::ShouldUpdateWriterConnection(HostInfo current_host) {
    return current_host.GetHostRole() == WRITER && writer_connection_->wrapped_dbc != current_connection_ && (!this->verify_new_conns_ || this->plugin_service_->GetHostListProvider()->GetConnectionRole(current_connection_) == WRITER);
}
