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

#ifndef SIMPLE_READ_WRITE_SPLITTING_PLUGIN_H
#define SIMPLE_READ_WRITE_SPLITTING_PLUGIN_H

#include "abstract_read_write_splitting_plugin.h"

class SimpleReadWriteSplittingPlugin : public AbstractReadWriteSplittingPlugin {
public:
    SimpleReadWriteSplittingPlugin(DBC *dbc);
    SimpleReadWriteSplittingPlugin(DBC *dbc, BasePlugin *next_plugin);

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

    DBC *GetVerifiedConnection(std::string host,
                               HOST_ROLE role, SQLHDBC ConnectionHandle, SQLHWND WindowHandle,
                               SQLTCHAR *OutConnectionString,
                               SQLSMALLINT BufferLength, SQLSMALLINT *StringLengthPtr, SQLUSMALLINT DriverCompletion);

    HostInfo CreateHostInfo(const std::string & string, HOST_ROLE reader);

    SQLRETURN InitializeReaderConnection() override;
    SQLRETURN InitializeWriterConnection() override;
    void RefreshAndStoreTopology() override;
    void CloseReaderIfNecessary(SQLHDBC current_conn) override;
    bool ShouldUpdateWriterConnection(HostInfo current_host) override;
    bool ShouldUpdateReaderConnection(HostInfo current_host) override;
    void SetInitialConnectionHostInfo(SQLHDBC conn, HostInfo host_info);

private:
    std::string write_endpoint;
    std::string read_endpoint;
    bool verify_new_conns_;
    std::chrono::milliseconds connect_retry_timeout_ms;
    std::chrono::milliseconds connect_retry_interval_ms;
    HOST_ROLE verify_initial_conn_type_;
};

#endif // SIMPLE_READ_WRITE_SPLITTING_PLUGIN_H