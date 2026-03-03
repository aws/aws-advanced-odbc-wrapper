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

#ifndef ABSTRACT_READ_WRITE_SPLITTING_PLUGIN_H
#define ABSTRACT_READ_WRITE_SPLITTING_PLUGIN_H

#include "../../host_info.h"
#include "../../util/odbc_helper.h"
#include "../../util/sliding_cache_map.h"
#include "../base_plugin.h"

class AbstractReadWriteSplittingPlugin : public BasePlugin {
public:
    AbstractReadWriteSplittingPlugin(DBC* dbc);
    AbstractReadWriteSplittingPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin);
    ~AbstractReadWriteSplittingPlugin();

    SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText = 0,
        SQLINTEGER     TextLength = -1) override;

    void ReleaseResources() override;

    void UpdateInternalConnectionInfo();

    void SetWriterConnection(DBC *conn, const HostInfo &host);

    void SetReaderConnection(DBC *conn, const HostInfo &host);

    SQLRETURN SwitchConnectionIfRequired(bool read_only, const HostInfo &current_host);

    void SwitchCurrentConnectionTo(DBC *new_conn, const HostInfo &new_host);

    SQLRETURN SwitchToWriterConnection(const HostInfo &current_host);

    SQLRETURN SwitchToReaderConnection(const HostInfo &current_host);

    std::pair<std::chrono::steady_clock::time_point, std::chrono::milliseconds> GetKeepAliveTimeout(
    );

    void CloseIdleConnections();

    void CloseReaderConnectionIfIdle();

    void CloseWriterConnectionIfIdle();

    void DisconnectAndFreeDBC(DBC *dbc, bool keep_dbc = false);

    void CloseReaderIfExpired();

    DBC *GetCurrentReaderConn();

    void SetStmtError(const std::string &msg, SQL_STATE_CODE state);

    virtual bool ShouldUpdateWriterConnection(const HostInfo &current_host) = 0;

    virtual bool ShouldUpdateReaderConnection(const HostInfo &current_host) = 0;

    virtual void CloseReaderIfNecessary() = 0;

    virtual SQLRETURN RefreshAndStoreTopology() = 0;

    virtual SQLRETURN InitializeWriterConnection() = 0;

    virtual SQLRETURN InitializeReaderConnection() = 0;

protected:
    std::shared_ptr<OdbcHelper> odbc_helper_;
    BasePlugin* plugin_head_ = nullptr;
    DBC* writer_connection_ = nullptr;
    CacheEntry<DBC*> reader_cache_item_ = CacheEntry<DBC*>();
    std::weak_ptr<PluginService> plugin_service_;
    HostInfo writer_host_info_ = HostInfo{};
    HostInfo reader_host_info_ = HostInfo{};
    std::map<std::string, std::string> connection_attributes_;
    SQLHENV henv_;
    SQLHDBC current_connection_ = nullptr;
    DBC* dbc_ = nullptr;

private:
    const std::chrono::milliseconds default_keep_alive_timeout_ = std::chrono::milliseconds(0);
    std::recursive_mutex lock_;
};

#endif // ABSTRACT_READ_WRITE_SPLITTING_PLUGIN_H
