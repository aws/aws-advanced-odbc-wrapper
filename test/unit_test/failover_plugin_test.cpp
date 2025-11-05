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

#include <gtest/gtest.h>

#include "failover_mock_objects.h"

class FailoverPluginTest : public ::testing::Test {};

TEST_F(FailoverPluginTest, TopologyMonitorReferenceCountingTest) {
    const std::string cluster_id = "test-cluster";

    ENV env;
    env.driver_lib_loader = std::make_shared<MockRdsLibLoader>();

    DBC* dbc = new DBC();
    dbc->env = &env;
    dbc.conn_attr[KEY_CLUSTER_ID] = AS_RDS_STR(cluster_id.c_str());

    auto mock_dialect = std::make_shared<MockDialect>();
    auto mock_host_selector = std::make_shared<MockHostSelector>();
    std::shared_ptr<ClusterTopologyQueryHelper> mock_topology_query_helper = std::make_shared<MockClusterTopologyQueryHelper>();

    {
        FailoverPlugin plugin1(dbc, nullptr, FailoverMode::STRICT_WRITER,
                              mock_dialect, mock_host_selector,
                              mock_topology_query_helper, nullptr);

        EXPECT_EQ(FailoverPlugin::GetTopologyMonitorCount(cluster_id), 1);

        FailoverPlugin plugin2(dbc, nullptr, FailoverMode::STRICT_WRITER,
                              mock_dialect, mock_host_selector,
                              mock_topology_query_helper, nullptr);
        EXPECT_EQ(FailoverPlugin::GetTopologyMonitorCount(cluster_id), 2);
    }

    EXPECT_EQ(FailoverPlugin::GetTopologyMonitorCount(cluster_id), 0);
    delete dbc;
}
