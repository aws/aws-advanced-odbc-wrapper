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

#ifndef TEST_COMMON_MOCK_OBJECTS_H
#define TEST_COMMON_MOCK_OBJECTS_H

#include "gmock/gmock.h"

#include <chrono>
#include <sqltypes.h>

#include "../../driver/host_list_providers/topology_util.h"
#include "../../driver/host_selector/highest_weight_host_selector.h"
#include "../../driver/util/plugin_service.h"

#include <chrono>

class MOCK_DIALECT : public Dialect {
public:
    MOCK_DIALECT() : Dialect() {};
    MOCK_METHOD(bool, IsSqlStateNetworkError, (const char* sql_state), ());
    MOCK_METHOD(bool, IsSqlStateAccessError, (const char* sql_state), (override));
    MOCK_METHOD(bool, IsSqlStateAccessError, (const char* sql_state, const std::string& error_message), (override));
};

class MOCK_HOST_SELECTOR : public HighestWeightHostSelector {
public:
    MOCK_HOST_SELECTOR() : HighestWeightHostSelector() {};
    MOCK_METHOD(HostInfo, GetHost, (std::vector<HostInfo> hosts, bool is_writer,
        std::string properties), ());
};

class MOCK_HOST_LIST_PROVIDER : public HostListProvider {
public:
    MOCK_HOST_LIST_PROVIDER(): HostListProvider("someClusterId") {};
    MOCK_METHOD(HostInfo, GetConnectionInfo, (SQLHDBC hdbc), ());
    MOCK_METHOD(HOST_ROLE, GetConnectionRole, (SQLHDBC hdbc), ());
};

class MockHostSelector : public HostSelector {
public:
    HostInfo GetHost(std::vector<HostInfo> hosts, bool is_writer, std::unordered_map<std::string, std::string> properties) override {
        return HostInfo();
    }
};

class MOCK_ODBC_HELPER : public OdbcHelper {
public:
    MOCK_ODBC_HELPER() : OdbcHelper(std::make_shared<RdsLibLoader>()) {};
    MOCK_METHOD(void, Disconnect, (DBC *dbc), (override));
    MOCK_METHOD(std::string, GetSqlStateAndLogMessage, (DBC *dbc), ());
    MOCK_METHOD(std::string, GetSqlStateAndLogMessage, (DBC *dbc, std::string& out_message), ());
    MOCK_METHOD(std::string, GetStmtErrorMessage, (SQLHSTMT stmt), ());
};

class MOCK_PLUGIN_SERVICE : public PluginService {
public:
    MOCK_PLUGIN_SERVICE() : PluginService() {}

    MOCK_METHOD(std::vector<HostInfo>, GetHosts, (), ());
    MOCK_METHOD(void, SetInitialHostInfo, (const HostInfo& info), ());
    MOCK_METHOD(void, ForceRefreshHosts, (bool verify_writer, std::chrono::milliseconds timeout_ms), ());
    MOCK_METHOD(std::shared_ptr<HostListProvider>, GetHostListProvider, (), ());
    MOCK_METHOD(std::shared_ptr<HostSelector>, GetHostSelector, (), ());
    MOCK_METHOD(std::shared_ptr<OdbcHelper>, GetOdbcHelper, (), ());
    MOCK_METHOD(std::shared_ptr<Dialect>, GetDialect, (), ());
    MOCK_METHOD(std::shared_ptr<TopologyUtil>, GetTopologyUtil, (), ());
};

class MOCK_TOPOLOGY_UTIL : public TopologyUtil {
public:
    MOCK_TOPOLOGY_UTIL() : TopologyUtil() {};
    MOCK_TOPOLOGY_UTIL(const std::shared_ptr<OdbcHelper> &odbc_helper, const std::shared_ptr<Dialect> &dialect) : TopologyUtil(odbc_helper, dialect) {};

    MOCK_METHOD(std::vector<HostInfo>, GetHosts, (SQLHSTMT stmt, const HostInfo& initial_host, const HostInfo& host_template), ());
    MOCK_METHOD(HostInfo, GetWriter, (const std::vector<HostInfo>& hosts), ());
};
#endif //TEST_COMMON_MOCK_OBJECTS_H
