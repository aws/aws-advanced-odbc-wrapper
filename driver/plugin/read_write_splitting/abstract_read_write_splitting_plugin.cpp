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

#include <algorithm>

#include "abstract_read_write_splitting_plugin.h"

#include "../../odbcapi_rds_helper.h"
#include "../../util/plugin_service.h"
#include "../../util/sql_query_analyzer.h"

const std::vector<std::string> FAILOVER_ERRORS = {
    "08S01", "08S02", "08007"
};

AbstractReadWriteSplittingPlugin::AbstractReadWriteSplittingPlugin(DBC *dbc) : AbstractReadWriteSplittingPlugin(dbc, nullptr) {}

AbstractReadWriteSplittingPlugin::AbstractReadWriteSplittingPlugin(DBC *dbc, BasePlugin *next_plugin) : BasePlugin(dbc, next_plugin) {
    this->plugin_name = "ABSTRACT_READ_WRITE_SPLITTING";
    this->plugin_service_ = dbc->plugin_service;
    this->odbc_helper_ = plugin_service_->GetOdbcHelper();
    this->connection_attributes_ = plugin_service_->GetOriginalConnAttr();
    this->next_plugin = next_plugin;
    this->dbc_ = dbc;

    henv_ = dbc_->env;
}

AbstractReadWriteSplittingPlugin::~AbstractReadWriteSplittingPlugin() {
    const std::lock_guard<std::recursive_mutex> lock_guard(lock_);

    if (DBC* reader_conn = this->reader_cache_item_.value; reader_conn && reader_conn != dbc_) {
        reader_conn->wrapped_dbc = nullptr;
        delete reader_conn;
    }
    this->reader_cache_item_ = CacheEntry<DBC*>();

    if (writer_connection_ && writer_connection_ != dbc_) {
        writer_connection_->wrapped_dbc = nullptr;
        delete writer_connection_;
    }
    this->writer_connection_ = nullptr;
}

void AbstractReadWriteSplittingPlugin::ReleaseResources() {
    const std::lock_guard<std::recursive_mutex> lock_guard(lock_);

    DBC* reader_conn = this->reader_cache_item_.value;
    if (reader_conn && reader_conn != dbc_) {
        if (reader_conn->wrapped_dbc && reader_conn->wrapped_dbc != dbc_->wrapped_dbc) {
            DisconnectAndFreeDBC(reader_conn);
        } else {
            reader_conn->wrapped_dbc = nullptr;
            delete reader_conn;
        }
    }
    this->reader_cache_item_ = CacheEntry<DBC*>();
    this->reader_host_info_ = HostInfo{};

    if (writer_connection_ && writer_connection_ != dbc_) {
        if (writer_connection_->wrapped_dbc && writer_connection_->wrapped_dbc != dbc_->wrapped_dbc) {
            DisconnectAndFreeDBC(writer_connection_);
        } else {
            writer_connection_->wrapped_dbc = nullptr;
            delete writer_connection_;
        }
    }
    this->writer_connection_ = nullptr;
    this->writer_host_info_ = HostInfo{};

    this->next_plugin->ReleaseResources();
}

SQLRETURN AbstractReadWriteSplittingPlugin::Execute(SQLHSTMT StatementHandle, SQLTCHAR *StatementText, SQLINTEGER TextLength) {
    LOG(INFO) << "Entering Execute";
    const std::string query = StatementText ? AS_UTF8_CSTR(StatementText) : "";
    const std::optional<bool> read_only = SqlQueryAnalyzer::DoesSetReadOnly(query, plugin_service_->GetDialect());
    const HostInfo curr_host = plugin_service_->GetCurrentHostInfo();

    STMT* stmt = static_cast<STMT*>(StatementHandle);
    const DBC* dbc = stmt->dbc;

    this->plugin_head_ = dbc->plugin_head;
    this->current_connection_ = dbc->wrapped_dbc;

    SQLRETURN ret = SQL_SUCCESS;
    if (read_only.has_value()) {
        {
            const std::lock_guard<std::recursive_mutex> lock_guard(lock_);
            ret = SwitchConnectionIfRequired(read_only.value(), curr_host);
        }
    }

    if (!SQL_SUCCEEDED(ret)) {
        return ret;
    }

    ret = next_plugin->Execute(StatementHandle, StatementText, TextLength);

    if (SQL_SUCCEEDED(ret)) {
        return ret;
    }

    std::string state = this->odbc_helper_->GetSqlStateAndLogMessage(nullptr, stmt);
    const bool failover_err = std::ranges::any_of(FAILOVER_ERRORS, [&state](const std::string &prefix) {
        return state.starts_with(prefix);
    });
    if (failover_err) {
        LOG(INFO) << "Detected a failover exception while executing a command: '" << query << "'";
        {
            const std::lock_guard<std::recursive_mutex> lock_guard(lock_);
            CloseIdleConnections();
            UpdateInternalConnectionInfo();
        }
    } else {
        LOG(INFO) << "Detected an exception while executing a command: '" << query << "'";
    }

    return ret;
}

void AbstractReadWriteSplittingPlugin::UpdateInternalConnectionInfo() {
    const HostInfo current_host = plugin_service_->GetCurrentHostInfo();
    if (current_host == HostInfo{}) {
        return;
    }

    this->current_connection_ = dbc_->wrapped_dbc;

    const bool update_writer = ShouldUpdateWriterConnection(current_host);
    const bool update_reader = ShouldUpdateReaderConnection(current_host);
    if (!update_writer && !update_reader) {
        return;
    }

    DBC* dbc = new DBC();
    dbc->env = dbc_->env;
    dbc->wrapped_dbc = dbc_->wrapped_dbc;
    {
        const std::lock_guard<std::recursive_mutex> lock_guard(dbc_->env->lock);
        dbc_->env->dbc_list.emplace_back(dbc);
    }

    if (update_writer) {
        SetWriterConnection(dbc, current_host);
    } else {
        SetReaderConnection(dbc, current_host);
    }
}

void AbstractReadWriteSplittingPlugin::SetWriterConnection(DBC* conn, const HostInfo &host) {
    if (this->writer_connection_ != nullptr) {
        DBC* old_writer = this->writer_connection_;
        this->writer_connection_ = nullptr;
        bool keep_dbc = false;
        if (old_writer->wrapped_dbc == current_connection_) {
            keep_dbc = true;
        }
        DisconnectAndFreeDBC(old_writer, keep_dbc);
    }
    this->writer_connection_ = conn;
    this->writer_host_info_ = host;
    LOG(INFO) << "Writer connection set to '"<< host.GetHostPortPair() <<"'";
}

void AbstractReadWriteSplittingPlugin::SetReaderConnection(DBC* conn, const HostInfo &host) {
    CloseReaderConnectionIfIdle();
    DBC* reader_conn = GetCurrentReaderConn();
    if (reader_conn != nullptr) {
        reader_cache_item_.value = nullptr;
        bool keep_dbc = false;
        if (reader_conn->wrapped_dbc == current_connection_) {
            keep_dbc = true;
        }
        DisconnectAndFreeDBC(reader_conn, keep_dbc);
    }

    const std::pair<std::chrono::steady_clock::time_point, std::chrono::milliseconds> timeout = GetKeepAliveTimeout();
    this->reader_cache_item_ = CacheEntry{.value=conn, .expiry=timeout.first, .time_to_expire_ms=timeout.second};
    this->reader_host_info_ = host;
    LOG(INFO) << "Reader connection set to '"<< host.GetHostPortPair() <<"'";

}

SQLRETURN AbstractReadWriteSplittingPlugin::SwitchConnectionIfRequired(bool read_only, const HostInfo &current_host) {
    if (current_connection_ != nullptr && !IsConnectionUsable(current_connection_)) {
        SetStmtError("Cannot switch connections with a closed connection.", ERR_RW_SWITCH_ON_CLOSED_CONN);
        return SQL_ERROR;
    }

    if (!SQL_SUCCEEDED(RefreshAndStoreTopology())) {
        return SQL_ERROR;
    }

    SQLRETURN ret = SQL_ERROR;
    if (read_only) {
        if (current_host.GetHostRole() != READER && dbc_->transaction_status != TRANSACTION_OPEN) {
            ret = SwitchToReaderConnection(current_host);
        }

        if (!SQL_SUCCEEDED(ret)) {
            if (!IsConnectionUsable(current_connection_)) {
                SetStmtError("An error occurred while trying to switch to a reader connection.", ERR_RW_READER_SWITCH_FAILED);
                return SQL_ERROR;
            }

            // Failed to switch to a reader. The current connection will be used as a fallback.
            LOG(INFO) << "Failed to switch to reader host. The current connection will be used as a fallback: '" << current_host.GetHost() << "'";
        }
    } else {
        if (current_host.GetHostRole() != WRITER && dbc_->transaction_status == TRANSACTION_OPEN) {
            SetStmtError("Switching to a read-write connection within a transaction. Please complete the transaction before switching connections.", ERR_RW_TX_SWITCH_FAILED);
            return SQL_ERROR;
        }

        if (current_host.GetHostRole() != WRITER) {
            ret = SwitchToWriterConnection(current_host);

            if (!SQL_SUCCEEDED(ret)) {
                SetStmtError("An error occurred while trying to switch to a writer connection.", ERR_RW_WRITER_SWITCH_FAILED);
                return SQL_ERROR;
            }
        }
    }

    return SQL_SUCCESS;
}

SQLRETURN AbstractReadWriteSplittingPlugin::SwitchToWriterConnection(const HostInfo &current_host) {
    if (current_host.GetHostRole() == WRITER && IsConnectionUsable(current_connection_)) {
        // Already connected to the writer.
        return SQL_SUCCESS;
    }

    SQLRETURN ret = SQL_ERROR;
    if (!this->writer_connection_ || !IsConnectionUsable(this->writer_connection_->wrapped_dbc)) {
        ret = InitializeWriterConnection();
    } else {
        SwitchCurrentConnectionTo(this->writer_connection_, this->writer_host_info_);
        ret = SQL_SUCCESS;
    }

    if (SQL_SUCCEEDED(ret)) {
        LOG(INFO) << "Switched from a reader to a writer host. New writer host: '" << this->writer_host_info_.GetHost() << "'";
    } else {
        LOG(INFO) << "Failed switching from a reader to a writer host. Attempted switch to writer host: '" << this->writer_host_info_.GetHost() << "'";
    }
    return ret;
}

void AbstractReadWriteSplittingPlugin::SwitchCurrentConnectionTo(DBC* new_conn, const HostInfo &new_host) {
    const SQLHDBC new_conn_wrapped = new_conn->wrapped_dbc;

    if (new_conn->wrapped_dbc == current_connection_) {
        return;
    }

    if (current_connection_ != nullptr) {
        {
            // Null out dbc_'s underlying statements, they can be reallocated in the default plugin using the new connection.
            const std::lock_guard<std::recursive_mutex> lock_guard_dbc(dbc_->lock);
            for (STMT* stmt : dbc_->stmt_list) {
                {
                    const std::lock_guard<std::recursive_mutex> lock_guard_stmt(stmt->lock);
                    if (stmt->wrapped_stmt) {
                        NULL_CHECK_CALL_LIB_FUNC(dbc_->env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
                            SQL_HANDLE_STMT, stmt->wrapped_stmt);
                        stmt->wrapped_stmt = nullptr;
                    }
                }
            }
        }

        // Free the current underlying connection. If reader_cache_item_ or writer_connection_ had the same underlying
        // connection, delete them.
        DBC* reader_conn = GetCurrentReaderConn();
        DBC* writer_conn = writer_connection_;
        const ENV* env = dbc_->env;
        if (const SQLHDBC old_wrapped = dbc_->wrapped_dbc) {
            const RdsLibResult disconnect_res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect, old_wrapped);
            if (SQL_SUCCEEDED(disconnect_res.fn_result)) {
                NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle, SQL_HANDLE_DBC, old_wrapped);
            }
            dbc_->wrapped_dbc = nullptr;

            if (writer_conn && writer_conn->wrapped_dbc == old_wrapped) {
                writer_conn->wrapped_dbc = nullptr;
                {
                    const std::lock_guard<std::recursive_mutex> lock_guard_env(dbc_->env->lock);
                    dbc_->env->dbc_list.remove(writer_conn);
                }
                delete this->writer_connection_;
                this->writer_connection_ = nullptr;
            }
            if (reader_conn && reader_conn->wrapped_dbc == old_wrapped) {
                reader_conn->wrapped_dbc = nullptr;
                {
                    const std::lock_guard<std::recursive_mutex> lock_guard_env(dbc_->env->lock);
                    dbc_->env->dbc_list.remove(reader_conn);
                }
                delete this->reader_cache_item_.value;
                this->reader_cache_item_ = CacheEntry<DBC*>();
            }
        }
    }

    this->current_connection_ = new_conn_wrapped;
    this->dbc_->wrapped_dbc = new_conn_wrapped;
    this->plugin_service_->SetCurrentHostInfo(new_host);
    LOG(INFO) << "Switched underlying connection.";
}

SQLRETURN AbstractReadWriteSplittingPlugin::SwitchToReaderConnection(const HostInfo &current_host) {
    if (current_host.GetHostRole() == READER && IsConnectionUsable(current_connection_)) {
        // Already connected to reader.
        return SQL_SUCCESS;
    }

    CloseReaderIfNecessary();

    DBC* reader_conn = GetCurrentReaderConn();
    SQLRETURN ret = SQL_ERROR;
    if (reader_conn == nullptr || !IsConnectionUsable(reader_conn->wrapped_dbc)) {
        ret = InitializeReaderConnection();
    } else {
        SwitchCurrentConnectionTo(reader_conn, reader_host_info_);
        LOG(INFO) << "Switched from a writer to a reader host. New reader host: '" << reader_host_info_.GetHost() << "'";
        ret = SQL_SUCCESS;
    }

    return ret;
}

// TODO: this was added to the OdbcHelper in the BG PR
bool AbstractReadWriteSplittingPlugin::IsConnectionUsable(SQLHDBC conn) {
    if (conn == nullptr) {
        return false;
    }

    SQLINTEGER len;
    SQLUINTEGER attr;
    const ENV* env = static_cast<ENV*>(henv_);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetConnectAttr, RDS_STR_SQLGetConnectAttr,
        conn, SQL_ATTR_CONNECTION_DEAD, &attr, 0, &len
    );
    SQLRETURN const ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, conn, res);
    return !SQL_SUCCEEDED(ret) || attr != SQL_CD_TRUE;
}

std::pair<std::chrono::steady_clock::time_point, std::chrono::milliseconds>
AbstractReadWriteSplittingPlugin::GetKeepAliveTimeout() {
    const std::chrono::milliseconds keep_alive_timeout = dbc_->conn_attr.contains(KEY_CACHED_READER_KEEP_ALIVE_TIMEOUT_MS) ?
       std::chrono::milliseconds(std::strtol(dbc_->conn_attr.at(KEY_CACHED_READER_KEEP_ALIVE_TIMEOUT_MS).c_str(), nullptr, 0))
       : default_keep_alive_timeout_;
    const std::chrono::steady_clock::time_point expiry_time = std::chrono::steady_clock::now() + keep_alive_timeout;
    return std::pair{expiry_time, keep_alive_timeout};
}

void AbstractReadWriteSplittingPlugin::CloseIdleConnections() {
    LOG(INFO) << "Closing all internal connections except for the current one.";
    CloseReaderConnectionIfIdle();
    CloseWriterConnectionIfIdle();
}

void AbstractReadWriteSplittingPlugin::CloseReaderConnectionIfIdle() {
    DBC* reader_conn = GetCurrentReaderConn();
    if (reader_conn != nullptr && reader_conn->wrapped_dbc != current_connection_
        && reader_conn->wrapped_dbc != dbc_->wrapped_dbc) {
        if (IsConnectionUsable(reader_conn->wrapped_dbc)) {
            // reader_conn is open but is not currently in use, so we close it.
            this->reader_cache_item_ = CacheEntry<DBC*>();
            this->reader_host_info_ = HostInfo{};
            DisconnectAndFreeDBC(reader_conn);
        }
    }
}

void AbstractReadWriteSplittingPlugin::CloseWriterConnectionIfIdle() {
    DBC* writer_conn = writer_connection_;
    if (writer_conn != nullptr && writer_conn->wrapped_dbc != current_connection_
        && writer_conn->wrapped_dbc != dbc_->wrapped_dbc) {
        if (IsConnectionUsable(writer_conn->wrapped_dbc)) {
            // writer_conn is open but is not currently in use, so we close it.
            this->writer_connection_ = nullptr;
            this->writer_host_info_ = HostInfo{};
            DisconnectAndFreeDBC(writer_conn);
        }
    }
}

void AbstractReadWriteSplittingPlugin::DisconnectAndFreeDBC(DBC* dbc, bool keep_dbc) {
    if (!dbc) {
        return;
    }

    this->odbc_helper_->Disconnect(dbc);

    if (!keep_dbc) {
        // RDS_FreeConnect handles statements, descriptors, wrapped_dbc,
        // dbc_list removal, error cleanup, and deletes the DBC.
        RDS_FreeConnect(dbc);
    } else {
        // Keep the DBC shell alive but detach the underlying handle.
        dbc->wrapped_dbc = nullptr;
    }
}

void AbstractReadWriteSplittingPlugin::CloseReaderIfExpired() {
    DBC* reader_conn = this->reader_cache_item_.value;
    if (reader_conn != nullptr
        && reader_conn->wrapped_dbc != current_connection_
        && reader_cache_item_.time_to_expire_ms != std::chrono::milliseconds(0)
        && std::chrono::steady_clock::now() > this->reader_cache_item_.expiry)
    {
        this->reader_cache_item_ = CacheEntry<DBC*>();
        this->reader_host_info_ = HostInfo{};
        DisconnectAndFreeDBC(reader_conn);
    }
}

DBC* AbstractReadWriteSplittingPlugin::GetCurrentReaderConn() {
    CloseReaderIfExpired();
    return reader_cache_item_.value;
}

void AbstractReadWriteSplittingPlugin::SetStmtError(const std::string &msg, SQL_STATE_CODE state) {
    LOG(ERROR) << msg;
    const std::lock_guard<std::recursive_mutex> lock_guard_dbc(dbc_->lock);
    for (STMT* stmt : dbc_->stmt_list) {
        const std::lock_guard<std::recursive_mutex> lock_guard_stmt(stmt->lock);
        if (stmt->wrapped_stmt) {
            NULL_CHECK_CALL_LIB_FUNC(dbc_->env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
                SQL_HANDLE_STMT, stmt->wrapped_stmt);
        }
        stmt->wrapped_stmt = nullptr;
        CLEAR_STMT_ERROR(stmt);
        stmt->err = new ERR_INFO(msg.c_str(), state);
    }
}
