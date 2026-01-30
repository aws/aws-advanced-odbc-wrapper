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

    auto mock_lib_loader = std::make_shared<MockRdsLibLoader>();
    auto mock_plugin_service = std::make_shared<MockPluginService>();

    ENV env;
    env.driver_lib_loader = mock_lib_loader;

    DBC* dbc = new DBC();
    dbc->env = &env;
    dbc->conn_attr[KEY_CLUSTER_ID] = cluster_id;
    dbc->plugin_service = mock_plugin_service;

    {
        FailoverPlugin plugin1(dbc, nullptr);
        FailoverPlugin plugin2(dbc, nullptr);
    }

    delete dbc;
}
