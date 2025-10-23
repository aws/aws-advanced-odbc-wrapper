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
#include <gmock/gmock.h>
#include "../../driver/plugin/failover/failover_plugin.h"

class FailoverPluginTest : public ::testing::Test {};

TEST_F(FailoverPluginTest, TopologyMonitorReferenceCountingTest) {
    const std::string cluster_id = "test-cluster";

    DBC dbc;
    dbc.conn_attr[KEY_CLUSTER_ID] = ToRdsStr(cluster_id);
    {
        FailoverPlugin plugin1(&dbc);

        EXPECT_EQ(FailoverPlugin::get_topology_monitors_count(), 1);

        FailoverPlugin plugin2(&dbc);
        EXPECT_EQ(FailoverPlugin::get_topology_monitors_count(), 2);
    }

    EXPECT_EQ(FailoverPlugin::get_topology_monitors_count(), 1);
}
