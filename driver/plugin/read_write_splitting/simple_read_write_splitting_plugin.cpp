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

#include "../../util/map_utils.h"
#include "../../util/plugin_service.h"
#include "../../util/rds_utils.h"

SimpleReadWriteSplittingPlugin::SimpleReadWriteSplittingPlugin(DBC *dbc) : SimpleReadWriteSplittingPlugin(dbc, nullptr) {}

SimpleReadWriteSplittingPlugin::SimpleReadWriteSplittingPlugin(DBC *dbc, BasePlugin *next_plugin) : AbstractReadWriteSplittingPlugin(dbc, next_plugin) {
    if (!dbc->conn_attr.contains(KEY_SRW_READ_ENDPOINT) || !dbc->conn_attr.contains(KEY_SRW_WRITE_ENDPOINT)) {
        throw std::runtime_error("Please ensure the SRW_WRITE_ENDPOINT and SRW_READ_ENDPOINT parameters have been set.");
    }
    this->read_endpoint = dbc->conn_attr.at(KEY_SRW_READ_ENDPOINT);
    this->write_endpoint = dbc->conn_attr.at(KEY_SRW_WRITE_ENDPOINT);
    this->verify_new_conns_ = MapUtils::GetBooleanValue(dbc->conn_attr, KEY_SRW_VERIFY_CONNS, false);
    this->verify_initial_conn_type_ = UNKNOWN;
    if (dbc->conn_attr.contains(KEY_SRW_VERIFY_INITIAL_CONN_TYPE)) {
        if (dbc->conn_attr.at(KEY_SRW_VERIFY_INITIAL_CONN_TYPE) == "WRITER") {
            this->verify_initial_conn_type_ = WRITER;
        } else if (dbc->conn_attr.at(KEY_SRW_VERIFY_INITIAL_CONN_TYPE) == "READER") {
            this->verify_initial_conn_type_ = READER;
        }
    }
    this->connect_retry_timeout_ms = MapUtils::GetMillisecondsValue(dbc->conn_attr, KEY_SRW_CONN_TIMEOUT_MS, DEFAULT_RETRY_TIMEOUT_MS);
    this->connect_retry_interval_ms = MapUtils::GetMillisecondsValue(dbc->conn_attr, KEY_SRW_CONN_INTERVAL_MS, DEFAULT_RETRY_INTERVAL_MS);
}

SQLRETURN SimpleReadWriteSplittingPlugin::Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) {
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    const bool skip_plugin = MapUtils::GetBooleanValue(dbc->conn_attr, KEY_SRW_SKIP, false);
    if (!this->verify_new_conns_ || skip_plugin) {
        // No verification required. Continue with a normal workflow.
        return next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    const std::string host = MapUtils::GetStringValue(dbc_->conn_attr, KEY_SERVER, "");

    DBC* conn = SQL_NULL_HDBC;
    if (RdsUtils::IsRdsWriterClusterDns(host) || this->verify_initial_conn_type_ == WRITER) {
        conn = GetVerifiedConnection(host, WRITER, ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    } else if (RdsUtils::IsRdsReaderClusterDns(host) || this->verify_initial_conn_type_ == READER) {
        conn = GetVerifiedConnection(host, READER, ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    SQLRETURN ret = SQL_ERROR;
    if (!conn) {
        // Continue with a normal workflow.
        ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
        conn = dbc;
    }

    if (SQL_SUCCEEDED(ret)) {
        SetInitialConnectionHostInfo(conn, host);
    }
    return ret;
}

void SimpleReadWriteSplittingPlugin::SetInitialConnectionHostInfo(SQLHDBC conn, std::string host) {
    const HOST_ROLE role = plugin_service_->GetHostListProvider()->GetConnectionRole(conn);
    const HostInfo host_info = CreateHostInfo(host, role);
    plugin_service_->SetInitialHostInfo(host_info);
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
    bool local_dbc_allocated = false;

    const std::chrono::time_point end_time = std::chrono::steady_clock::now() + this->connect_retry_timeout_ms;
    while (std::chrono::steady_clock::now() < end_time) {
        if (ConnectionHandle != SQL_NULL_HDBC) {
            ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
            if (SQL_SUCCEEDED(ret)) {
                conn = static_cast<DBC*>(ConnectionHandle);
            }
        } else if (!host.empty()) {
            // Allocate the local DBC once, reuse on retries to avoid rapid alloc/free cycles
            // that can corrupt the underlying ODBC driver's internal state.
            if (!local_dbc_allocated || !conn) {
                this->odbc_helper_->AllocDbc(henv_, local_hdbc);
                conn = static_cast<DBC*>(local_hdbc);
                local_dbc_allocated = true;
            }
            conn->conn_attr = connection_attributes_;
            conn->conn_attr.insert_or_assign(KEY_SRW_SKIP, VALUE_BOOL_TRUE); // Skip this plugin.
            conn->conn_attr.insert_or_assign(KEY_SERVER, host);
            ret = plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
            if (SQL_SUCCEEDED(ret)) {
                conn->conn_attr.erase(KEY_SRW_SKIP);
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

        if (conn == SQL_NULL_HDBC || !SQL_SUCCEEDED(ret) || plugin_service_->GetHostListProvider()->GetConnectionRole(conn) != role) {
            // The connection does not have the desired role. Retry.
            if (conn != SQL_NULL_HDBC && conn == static_cast<DBC*>(ConnectionHandle) && SQL_SUCCEEDED(ret)) {
                // Caller-owned handle connected but wrong role, disconnect before retrying
                this->odbc_helper_->Disconnect(static_cast<DBC*>(ConnectionHandle));
            } else if (conn != SQL_NULL_HDBC && local_dbc_allocated && SQL_SUCCEEDED(ret)) {
                // Local DBC connected but wrong role — disconnect but keep the DBC for reuse.
                // DefaultPlugin::Connect will reallocate wrapped_dbc on the next iteration.
                this->odbc_helper_->Disconnect(conn);
                if (conn->wrapped_dbc) {
                    NULL_CHECK_CALL_LIB_FUNC(dbc_->env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
                        SQL_HANDLE_DBC, conn->wrapped_dbc);
                    conn->wrapped_dbc = nullptr;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(this->connect_retry_interval_ms));
            continue;
        }

        // Connection is valid and verified.
        return conn;
    }

    // Timed out — free the local DBC if we allocated one
    if (local_dbc_allocated) {
        DisconnectAndFreeDBC(conn);
        conn = SQL_NULL_HDBC;
    }

    LOG(ERROR) << "The plugin was unable to establish a " << (role == READER ? "read only" : "read write") << " connection within " << this->connect_retry_timeout_ms.count() << " ms.";
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
        conn->conn_attr.insert_or_assign(KEY_SRW_SKIP, VALUE_BOOL_TRUE); // Skip this plugin.
        ret = plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(ret)) {
            odbc_helper_->DisconnectAndFree(&local_hdbc);
            conn = SQL_NULL_HDBC;
        } else {
            {
                const std::lock_guard<std::recursive_mutex> lock_guard(dbc_->env->lock);
                dbc_->env->dbc_list.remove(conn);
            }
            conn->conn_attr.erase(KEY_SRW_SKIP);
        }
    }
    if (conn == SQL_NULL_HDBC) {
        const std::string msg = "Failed to connect to reader host: '" + this->reader_host_info_.GetHostPortPair() + "', staying on current connection as fallback.";
        LOG(ERROR) << msg;
        if (!current_connection_) {
            SetStmtError(msg, ERR_RW_READER_SWITCH_FAILED);
            return SQL_ERROR;
        }
        return SQL_SUCCESS;
    }

    LOG(INFO) << "Successfully connected to a new reader host: '" << this->reader_host_info_.GetHostPortPair() << "'";
    SetReaderConnection(conn, reader_host_info_);
    SwitchCurrentConnectionTo(conn, reader_host_info_);
    LOG(INFO) << "Switched from a writer to a reader host. New reader host: '" << this->read_endpoint << "'";
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
        conn->conn_attr.insert_or_assign(KEY_SRW_SKIP, VALUE_BOOL_TRUE); // Skip this plugin.
        ret = plugin_head_->Connect(local_hdbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(ret)) {
            odbc_helper_->DisconnectAndFree(&local_hdbc);
            conn = SQL_NULL_HDBC;
        } else {
            conn->conn_attr.erase(KEY_SRW_SKIP);
            {
                const std::lock_guard<std::recursive_mutex> lock_guard(dbc_->env->lock);
                dbc_->env->dbc_list.remove(conn);
            }
        }
    }

    if (conn == SQL_NULL_HDBC) {
        SetStmtError("A writer connection was requested, but the plugin was unable to establish a writer connection with the writer endpoint '" + this->write_endpoint + "'.", ERR_RW_WRITER_SWITCH_FAILED);
        return SQL_ERROR;
    }

    SetWriterConnection(conn, this->writer_host_info_);
    SwitchCurrentConnectionTo(conn, this->writer_host_info_);
    return SQL_SUCCESS;
}

HostInfo SimpleReadWriteSplittingPlugin::CreateHostInfo(const std::string &endpoint, HOST_ROLE role) {
    const int port = MapUtils::GetIntValue(dbc_->conn_attr, KEY_PORT, plugin_service_->GetDialect()->GetDefaultPort());
    return {endpoint, port, UP, role};
}

SQLRETURN SimpleReadWriteSplittingPlugin::RefreshAndStoreTopology() {
    // Simple Read/Write Splitting does not rely on topology.
    return SQL_SUCCESS;
}

void SimpleReadWriteSplittingPlugin::CloseReaderIfNecessary() {
    // Simple Read/Write will connect to the reader endpoint regardless.
}

bool SimpleReadWriteSplittingPlugin::ShouldUpdateReaderConnection(const HostInfo &current_host) {
    const DBC* cached_reader_conn = GetCurrentReaderConn();
    return current_connection_ != nullptr &&
        current_host.GetHostRole() == READER &&
        cached_reader_conn != nullptr &&
        cached_reader_conn->wrapped_dbc != current_connection_ &&
        (!this->verify_new_conns_ || plugin_service_->GetHostListProvider()->GetConnectionRole(current_connection_) == READER);
}

bool SimpleReadWriteSplittingPlugin::ShouldUpdateWriterConnection(const HostInfo &current_host) {
    return current_connection_ != nullptr &&
        current_host.GetHostRole() == WRITER &&
        writer_connection_ != nullptr &&
        writer_connection_->wrapped_dbc != current_connection_ &&
        (!this->verify_new_conns_ || plugin_service_->GetHostListProvider()->GetConnectionRole(current_connection_) == WRITER);
}
