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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../driver/host_info.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/util/topology_service.h"

class TopologyServiceTest : public testing::Test {
protected:
    std::shared_ptr<TopologyService> topology_service;
    std::vector<HostInfo> all_hosts;
    HostInfo host_a = HostInfo("host_a.region.com", 1234, UP, false, 0);
    HostInfo host_b = HostInfo("host_b.region.com", 1234, UP, false, 0);
    HostInfo host_c = HostInfo("host_c.region.com", 1234, UP, false, 0);

    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    void SetUp() override {
        all_hosts.push_back(host_a);
        all_hosts.push_back(host_b);
        all_hosts.push_back(host_c);
        topology_service = std::make_shared<TopologyService>("ClusterId");
        topology_service->SetHosts(all_hosts);
    }
    void TearDown() override {}
};

TEST_F(TopologyServiceTest, GetHosts) {
    std::vector<HostInfo> topology_hosts = topology_service->GetHosts();
    EXPECT_EQ(all_hosts, topology_hosts);
}

TEST_F(TopologyServiceTest, GetHosts_WithFilter) {
    HostFilter filter;
    filter.blocked_host_ids.insert(host_a.GetHostId());
    filter.blocked_host_ids.insert(host_b.GetHostId());
    filter.blocked_host_ids.insert(host_c.GetHostId());
    topology_service->SetHostFilter(filter);

    std::vector<HostInfo> topology_hosts = topology_service->GetHosts();
    EXPECT_EQ(all_hosts, topology_hosts);
}

TEST_F(TopologyServiceTest, GetFilteredHosts_Allowed) {
    std::vector<HostInfo> expected_hosts;
    expected_hosts.push_back(host_a);

    HostFilter filter;
    filter.allowed_host_ids.insert(host_a.GetHostId());
    topology_service->SetHostFilter(filter);

    std::vector<HostInfo> topology_hosts = topology_service->GetFilteredHosts();
    EXPECT_EQ(expected_hosts, topology_hosts);
}

TEST_F(TopologyServiceTest, GetFilteredHosts_Blocked) {
    std::vector<HostInfo> expected_hosts;
    expected_hosts.push_back(host_b);
    expected_hosts.push_back(host_c);

    HostFilter filter;
    filter.blocked_host_ids.insert(host_a.GetHostId());
    topology_service->SetHostFilter(filter);

    std::vector<HostInfo> topology_hosts = topology_service->GetFilteredHosts();
    EXPECT_EQ(expected_hosts, topology_hosts);
}

TEST_F(TopologyServiceTest, InitClusterId_UserInput) {
    std::map<std::string, std::string> conn_info;
    std::string expected_id = "custom_id";
    conn_info.insert_or_assign(KEY_CLUSTER_ID, expected_id);
    std::string returned_id = TopologyService::InitClusterId(conn_info);
    EXPECT_EQ(returned_id, expected_id);
}

TEST_F(TopologyServiceTest, InitClusterId_Generated) {
    std::map<std::string, std::string> conn_info;
    conn_info.insert_or_assign(KEY_SERVER, "NON_RDS");
    std::string returned_id = TopologyService::InitClusterId(conn_info);
    EXPECT_FALSE(returned_id.empty());
    std::string map_id = conn_info.at(KEY_CLUSTER_ID);
    EXPECT_EQ(map_id, returned_id);
}
