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
    AbstractReadWriteSplittingPlugin(DBC* dbc, BasePlugin* next_plugin);
    ~AbstractReadWriteSplittingPlugin();

    SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText = 0,
        SQLINTEGER     TextLength = -1) override;

    void NotifyConnectionChanged() override;

    void UpdateInternalConnectionInfo();

    void SetWriterConnection(DBC *conn, HostInfo host);

    void SetReaderConnection(DBC *conn, HostInfo host);

    void SwitchConnectionIfRequired(bool read_only, const HostInfo &current_host);

    void SwitchCurrentConnectionTo(DBC *new_conn, HostInfo new_host);

    SQLRETURN SwitchToWriterConnection(HostInfo current_host);

    SQLRETURN SwitchToReaderConnection(HostInfo current_host);

    bool IsConnectionUsable(SQLHDBC conn);

    std::pair<std::chrono::steady_clock::time_point, std::chrono::milliseconds> GetKeepAliveTimeout(
        bool is_pooled_connection);

    void CloseIdleConnections();

    void CloseReaderConnectionIfIdle();

    void CloseWriterConnectionIfIdle();

    virtual bool ShouldUpdateWriterConnection(HostInfo current_host) = 0;

    virtual bool ShouldUpdateReaderConnection(HostInfo current_host) = 0;

    virtual void CloseReaderIfNecessary(SQLHDBC current_conn) = 0;

    virtual void RefreshAndStoreTopology() = 0;

    virtual SQLRETURN InitializeWriterConnection() = 0;

    virtual SQLRETURN InitializeReaderConnection() = 0;

protected:
    std::shared_ptr<OdbcHelper> odbc_helper_;
    BasePlugin* plugin_head_;
    DBC* writer_connection_;
    CacheEntry<DBC*> reader_cache_item_;
    std::shared_ptr<PluginService> plugin_service_;
    HostInfo writer_host_info_;
    HostInfo reader_host_info_;
    std::map<std::string, std::string> connection_attributes_;
    SQLHENV henv_;
    SQLHDBC current_connection_;
    DBC* dbc_;
    bool is_pooled_connection_;
    STMT* current_stmt_;

private:
    std::chrono::milliseconds keep_alive_timeout_ = std::chrono::minutes(0);
};

#endif // ABSTRACT_READ_WRITE_SPLITTING_PLUGIN_H