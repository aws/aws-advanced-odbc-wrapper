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

#include <iostream>
#include "abstract_read_write_splitting_plugin.h"

#include "../../odbcapi_rds_helper.h"
#include "../../util/plugin_service.h"

#include "../../util/sql_query_analyzer.h"

const std::vector<std::string> FAILOVER_ERRORS = {
    "08S01", "08S02", "08007"
};

AbstractReadWriteSplittingPlugin::AbstractReadWriteSplittingPlugin(DBC *dbc) : AbstractReadWriteSplittingPlugin(dbc, nullptr) {}

AbstractReadWriteSplittingPlugin::AbstractReadWriteSplittingPlugin(DBC *dbc, BasePlugin *next_plugin) {
    this->plugin_name = "ABSTRACT_READ_WRITE_SPLITTING";
    this->plugin_service_ = dbc->plugin_service;
    this->current_connection_ = nullptr;
    this->writer_connection_ = nullptr;
    this->writer_host_info_ = HostInfo{};
    this->reader_host_info_ = HostInfo{};
    this->reader_cache_item_ = CacheEntry<DBC*>();
    this->odbc_helper_ = plugin_service_->GetOdbcHelper();
    this->plugin_head_ = dbc->plugin_head;
    this->connection_attributes_ = this->plugin_service_->GetOriginalConnAttr();
    this->next_plugin = next_plugin;
    this->dbc_ = dbc;

    henv_ = dbc_->env;
    ENV* henv = static_cast<ENV*>(henv_);
    if (dbc->conn_attr.contains("RW_POOLING") && dbc->conn_attr.at("RW_POOLING") == VALUE_BOOL_TRUE) {
        odbc_helper_->SetEnvAttr(henv, SQL_ATTR_CONNECTION_POOLING, (SQLPOINTER)SQL_CP_ONE_PER_HENV, SQL_IS_INTEGER);
        this->is_pooled_connection_ = true;
    }
}

AbstractReadWriteSplittingPlugin::~AbstractReadWriteSplittingPlugin() {
    DBC* reader_conn = this->reader_cache_item_.Get();
    if (reader_conn && reader_conn->wrapped_dbc && reader_conn->wrapped_dbc != current_connection_) {
        odbc_helper_->DisconnectAndFreeDBC(reader_conn);
    }

    if (writer_connection_ && writer_connection_->wrapped_dbc && writer_connection_->wrapped_dbc != current_connection_) {
        odbc_helper_->DisconnectAndFreeDBC(writer_connection_);
    }
}

SQLRETURN AbstractReadWriteSplittingPlugin::Execute(SQLHSTMT StatementHandle, SQLTCHAR *StatementText, SQLINTEGER TextLength) {
    LOG(INFO) << "Entering Execute";
    const std::string query = StatementText ? AS_UTF8_CSTR(StatementText) : "";
    std::pair<bool, bool> read_only = SqlQueryAnalyzer::DoesSetReadOnly(query, this->plugin_service_->GetDialect());

    STMT* stmt = static_cast<STMT*>(StatementHandle);
    current_stmt_ = stmt;
    DBC* dbc = stmt->dbc;

    HostInfo curr_host = this->plugin_service_->GetCurrentHostInfo();

    this->plugin_head_ = dbc->plugin_head;
    this->current_connection_ = dbc->wrapped_dbc;

    SQLHSTMT hstmt = StatementHandle;
    if (read_only.first) {
        SwitchConnectionIfRequired(read_only.second, curr_host);
        SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &hstmt);
    }

    const SQLRETURN ret = next_plugin->Execute(hstmt, StatementText, TextLength);

    if (SQL_SUCCEEDED(ret)) {
        return ret;
    }

    SQLHSTMT stmt1 = SQL_NULL_HANDLE;
    const RdsLibResult res = this->odbc_helper_->BaseAllocStmt(&dbc_->wrapped_dbc, &stmt1);
    SQLSMALLINT stmt_length;
    SQLINTEGER native_error;
    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = { 0 };
    SQLTCHAR message[MAX_MSG_LEN] = { 0 };
    RDS_SQLError(nullptr, nullptr, stmt1, sql_state, &native_error, message, MAX_MSG_LEN, &stmt_length, true);

    std::string state(AS_UTF8_CSTR(sql_state));
    const bool failover_err = std::ranges::any_of(FAILOVER_ERRORS, [&state](const std::string &prefix) {
        return state.rfind(prefix, 0) == 0;
    });
    if (failover_err) {
        LOG(INFO) << "Detected a failover exception while executing a command: '" << query << "'";
        CloseIdleConnections();
    } else {
        LOG(INFO) << "Detected an exception while executing a command: '" << query << "'";
    }

    return SQL_ERROR;
}

void AbstractReadWriteSplittingPlugin::NotifyConnectionChanged() {
    UpdateInternalConnectionInfo();
}

void AbstractReadWriteSplittingPlugin::UpdateInternalConnectionInfo() {
    HostInfo current_host = this->plugin_service_->GetCurrentHostInfo();
    if (current_host == HostInfo{}) {
        return;
    }

    DBC* dbc = new DBC();
    dbc->env = dbc_->env;
    dbc->wrapped_dbc = dbc_->wrapped_dbc;
    {
        const std::lock_guard<std::recursive_mutex> lock_guard(dbc_->env->lock);
        dbc_->env->dbc_list.emplace_back(dbc);
    }

    if (ShouldUpdateWriterConnection(current_host)) {
        SetWriterConnection(dbc, current_host);
    } else if (ShouldUpdateReaderConnection(current_host)) {
        SetReaderConnection(dbc, current_host);
    }
}

void AbstractReadWriteSplittingPlugin::SetWriterConnection(DBC* conn, HostInfo host) {
    if (this->writer_connection_ != nullptr) {
        DBC* old_writer = this->writer_connection_;
        this->writer_connection_ = nullptr;
        bool keep_dbc = false;
        if (old_writer->wrapped_dbc == current_connection_) {
            keep_dbc = true;
        }
        odbc_helper_->DisconnectAndFreeDBC(old_writer, keep_dbc);
    }
    this->writer_connection_ = conn;
    this->writer_host_info_ = host;
    LOG(INFO) << "Writer connection set to '"<< host.GetHostPortPair() <<"'";
}

void AbstractReadWriteSplittingPlugin::SetReaderConnection(DBC* conn, HostInfo host) {
    CloseReaderConnectionIfIdle();
    DBC* reader_conn = this->reader_cache_item_.Get();
    if (reader_conn != nullptr) {
        reader_cache_item_.value = nullptr;
        bool keep_dbc = false;
        if (reader_conn->wrapped_dbc == current_connection_) {
            keep_dbc = true;
        }
        odbc_helper_->DisconnectAndFreeDBC(reader_conn, keep_dbc);
    }

    std::pair<std::chrono::steady_clock::time_point, std::chrono::milliseconds> timeout = GetKeepAliveTimeout(false);
    this->reader_cache_item_ = CacheEntry{conn, timeout.first, timeout.second};
    this->reader_host_info_ = host;
    LOG(INFO) << "Reader connection set to '"<< host.GetHostPortPair() <<"'";

}

void AbstractReadWriteSplittingPlugin::SwitchConnectionIfRequired(bool read_only, const HostInfo& current_host) {
    if (current_connection_ != nullptr && !IsConnectionUsable(current_connection_)) {
        LOG(ERROR) << "Cannot switch connections with a closed connection.";
        throw std::runtime_error("Cannot switch connections with a closed connection.");
    }

    RefreshAndStoreTopology();

    SQLRETURN ret = SQL_ERROR;
    if (read_only) {
        if (current_host.GetHostRole() != READER && dbc_->transaction_status != TRANSACTION_OPEN) {
            ret = SwitchToReaderConnection(current_host);
        }

        if (!SQL_SUCCEEDED(ret)) {
            if (!IsConnectionUsable(current_connection_)) {
                LOG(ERROR) << "An error occurred while trying to switch to a reader connection.";
                throw std::runtime_error("An error occurred while trying to switch to a reader connection.");
            }

            // Failed to switch to a reader. The current connection will be used as a fallback.
            LOG(INFO) << "Failed to switch to reader host. The current connection will be used as a fallback: '" << current_host.GetHost() << "'";
        }
    } else {
        if (current_host.GetHostRole() != WRITER && dbc_->transaction_status == TRANSACTION_OPEN) {
            LOG(ERROR) << "Switching to a read-write connection within a transaction. Please complete the transaction before switching connections.";
            throw std::runtime_error("Switching to a read-write connection within a transaction. Please complete the transaction before switching connections.");
        }

        if (current_host.GetHostRole() != WRITER) {
            ret = SwitchToWriterConnection(current_host);

            if (!SQL_SUCCEEDED(ret)) {
                LOG(ERROR) << "An error occurred while trying to switch to a writer connection.";
                throw std::runtime_error("An error occurred while trying to switch to a writer connection.");
            }
        }
    }
}

SQLRETURN AbstractReadWriteSplittingPlugin::SwitchToWriterConnection(HostInfo current_host) {
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

    if (this->is_pooled_connection_) {
        CloseReaderConnectionIfIdle();
    }

    LOG(INFO) << "Switched from a reader to a writer host. New writer host: '" + this->writer_host_info_.GetHost() + "'";
    return ret;
}

void AbstractReadWriteSplittingPlugin::SwitchCurrentConnectionTo(DBC* new_conn, HostInfo new_host) {
    if (new_conn->wrapped_dbc == current_connection_) {
        return;
    }

    if (current_connection_ != nullptr) {
        DBC* reader_conn = this->reader_cache_item_.Get();
        DBC* writer_conn = writer_connection_;
        if (writer_conn != dbc_ && writer_connection_ && current_connection_ == writer_connection_->wrapped_dbc) {
            this->writer_connection_ = nullptr;
            odbc_helper_->DisconnectAndFreeDBC(writer_conn);
        } else if (reader_conn != dbc_ && reader_conn && current_connection_ == reader_conn->wrapped_dbc) {
            this->reader_cache_item_.value = nullptr;
            odbc_helper_->DisconnectAndFreeDBC(reader_conn);
        } else {
            odbc_helper_->DisconnectAndFreeDBC(this->dbc_, true);
        }
    }
    this->current_connection_ = new_conn->wrapped_dbc;
    this->dbc_->wrapped_dbc = new_conn->wrapped_dbc;
    this->plugin_service_->SetCurrentHostInfo(new_host);
    LOG(INFO) << "Switched underlying connection.";
}

SQLRETURN AbstractReadWriteSplittingPlugin::SwitchToReaderConnection(HostInfo current_host) {
    if (current_host.GetHostRole() == READER && IsConnectionUsable(current_connection_)) {
        // Already connected to reader.
        return SQL_SUCCESS;
    }

    CloseReaderIfNecessary(current_connection_);

    DBC* reader_conn = this->reader_cache_item_.Get();
    SQLRETURN ret = SQL_ERROR;
    if (reader_conn == nullptr || !IsConnectionUsable(reader_conn->wrapped_dbc)) {
        ret = InitializeReaderConnection();
    } else {
        SwitchCurrentConnectionTo(reader_conn, current_host);
        LOG(INFO) << "Switched from a writer to a reader host. New reader host: '" + reader_host_info_.GetHost() + "'";
        ret = SQL_SUCCESS;
    }

    if (this->is_pooled_connection_) {
        CloseWriterConnectionIfIdle();
    }

    return ret;
}

bool AbstractReadWriteSplittingPlugin::IsConnectionUsable(SQLHDBC conn) {
    if (conn == nullptr) {
        return false;
    }

    SQLINTEGER len;
    SQLUINTEGER attr;
    ENV* env = static_cast<ENV*>(henv_);
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(env->driver_lib_loader, RDS_FP_SQLGetConnectAttr, RDS_STR_SQLGetConnectAttr,
        conn, SQL_ATTR_CONNECTION_DEAD, &attr, 0, &len
    );
    SQLRETURN ret = RDS_ProcessLibRes(SQL_HANDLE_DBC, conn, res);
    return !(SQL_SUCCEEDED(ret) && attr == SQL_CD_TRUE);
}

std::pair<std::chrono::steady_clock::time_point, std::chrono::milliseconds>
AbstractReadWriteSplittingPlugin::GetKeepAliveTimeout(bool is_pooled_connection) {
    if (is_pooled_connection) {
        // Let the connection pool handle the lifetime of the reader connection.
        return std::pair{std::chrono::steady_clock::now(), std::chrono::milliseconds{0}};
    }

    const std::chrono::milliseconds keep_alive_timeout = dbc_->conn_attr.contains(KEY_CACHED_READER_KEEP_ALIVE_TIMEOUT_MS) ?
       std::chrono::milliseconds(std::strtol(dbc_->conn_attr.at(KEY_CACHED_READER_KEEP_ALIVE_TIMEOUT_MS).c_str(), nullptr, 0))
       : std::chrono::milliseconds(0);
    const std::chrono::steady_clock::time_point expiry_time = std::chrono::steady_clock::now() + keep_alive_timeout;
    return std::pair{expiry_time, keep_alive_timeout};
}

void AbstractReadWriteSplittingPlugin::CloseIdleConnections() {
    LOG(INFO) << "Closing all internal connections except for the current one.";
    CloseReaderConnectionIfIdle();
    CloseWriterConnectionIfIdle();
}

void AbstractReadWriteSplittingPlugin::CloseReaderConnectionIfIdle() {
    DBC* reader_conn = this->reader_cache_item_.Get();
    if (reader_conn != nullptr && reader_conn != current_connection_) {
        if (IsConnectionUsable(reader_conn)) {
            // reader_conn is open but is not currently in use, so we close it.
            odbc_helper_->DisconnectAndFreeDBC(reader_conn);
            this->reader_cache_item_.value = nullptr;
        }
    }
}

void AbstractReadWriteSplittingPlugin::CloseWriterConnectionIfIdle() {
    if (writer_connection_ != nullptr && writer_connection_->wrapped_dbc != current_connection_) {
        if (IsConnectionUsable(writer_connection_)) {
            // writer_conn is open but is not currently in use, so we close it.
            this->odbc_helper_->DisconnectAndFreeDBC(writer_connection_);
            this->writer_connection_ = nullptr;
        }
    }
}
