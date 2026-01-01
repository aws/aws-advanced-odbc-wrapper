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

#ifndef FAILOVER_MOCK_OBJECTS_H_
#define FAILOVER_MOCK_OBJECTS_H_

#include "gmock/gmock.h"

#include "../../driver/dialect/dialect.h"
#include "../../driver/util/rds_lib_loader.h"
#include "../../driver/util/topology_service.h"
#include "../../driver/host_selector/host_selector.h"
#include "../../driver/plugin/failover/cluster_topology_query_helper.h"
#include "../../driver/plugin/failover/cluster_topology_monitor.h"
#include "../../driver/plugin/failover/failover_plugin.h"

class MockDialect : public Dialect {};

SQLRETURN MockFunction() {
    return SQL_SUCCESS;
}

class MockRdsLibLoader : public RdsLibLoader {
    public:
        FUNC_HANDLE GetFunction(const std::string& function_name) override {
            return reinterpret_cast<FUNC_HANDLE>(&MockFunction);
        }
};

class MockHostSelector : public HostSelector {
    public:
        HostInfo GetHost(std::vector<HostInfo> hosts, bool is_writer, std::unordered_map<std::string, std::string> properties) override {
            return HostInfo();
        }
};

class MockTopologyService : public TopologyService {
    public:
        MockTopologyService() : TopologyService("") {}
        std::vector<HostInfo> GetHosts() override { return {}; }
        void SetHosts(std::vector<HostInfo>) override {}
};

class MockClusterTopologyQueryHelper : public ClusterTopologyQueryHelper {
    public:
        MockClusterTopologyQueryHelper() : ClusterTopologyQueryHelper(nullptr, 0, "", "", "", "", std::make_shared<OdbcHelper>(nullptr)) {}
        std::vector<HostInfo> QueryTopology(SQLHDBC hdbc) override { return {}; }
};

#endif // FAILOVER_MOCK_OBJECTS_H_
