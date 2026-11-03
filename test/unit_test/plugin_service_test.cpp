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
#include "../../driver/util/plugin_service.h"

class PluginServiceTest : public testing::Test {
protected:
    std::shared_ptr<PluginService> plugin_service_;
    std::vector<HostInfo> all_hosts_;
    HostInfo host_a_ = HostInfo("host_a.region.com", 1234, UP, READER, 0);
    HostInfo host_b_ = HostInfo("host_b.region.com", 1234, UP, READER, 0);
    HostInfo host_c_ = HostInfo("host_c.region.com", 1234, UP, READER, 0);

    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    void SetUp() override {
        all_hosts_.push_back(host_a_);
        all_hosts_.push_back(host_b_);
        all_hosts_.push_back(host_c_);
        // Topology and host filters are process wide, use keys no other test shares
        const std::map<std::string, std::string> conn_attr = {
            {KEY_CLUSTER_ID, TEST_CLUSTER_ID},
            {KEY_SERVER, TEST_ENDPOINT},
        };
        plugin_service_ = std::make_shared<PluginService>(nullptr, nullptr, conn_attr, "ClusterId");
        plugin_service_->SetHosts(all_hosts_);
    }
    void TearDown() override {
        plugin_service_->SetHosts({});
        plugin_service_->SetHostFilter({});
    }

    static inline const std::string TEST_CLUSTER_ID = "plugin-service-test-cluster";
    static inline const std::string TEST_ENDPOINT = "plugin-service-test.cluster-custom-xyz.us-east-1.rds.amazonaws.com";
};

TEST_F(PluginServiceTest, GetHosts) {
    std::vector<HostInfo> topology_hosts = plugin_service_->GetHosts();
    EXPECT_EQ(all_hosts_, topology_hosts);
}

TEST_F(PluginServiceTest, GetHosts_WithFilter) {
    HostFilter filter;
    filter.blocked_host_ids.insert(host_a_.GetHostId());
    filter.blocked_host_ids.insert(host_b_.GetHostId());
    filter.blocked_host_ids.insert(host_c_.GetHostId());
    plugin_service_->SetHostFilter(filter);

    std::vector<HostInfo> topology_hosts = plugin_service_->GetHosts();
    EXPECT_EQ(all_hosts_, topology_hosts);
}

TEST_F(PluginServiceTest, GetFilteredHosts_Allowed) {
    std::vector<HostInfo> expected_hosts;
    expected_hosts.push_back(host_a_);

    HostFilter filter;
    filter.allowed_host_ids.insert(host_a_.GetHostId());
    plugin_service_->SetHostFilter(filter);

    std::vector<HostInfo> topology_hosts = plugin_service_->GetFilteredHosts();
    EXPECT_EQ(expected_hosts, topology_hosts);
}

TEST_F(PluginServiceTest, GetFilteredHosts_Blocked) {
    std::vector<HostInfo> expected_hosts;
    expected_hosts.push_back(host_b_);
    expected_hosts.push_back(host_c_);

    HostFilter filter;
    filter.blocked_host_ids.insert(host_a_.GetHostId());
    plugin_service_->SetHostFilter(filter);

    std::vector<HostInfo> topology_hosts = plugin_service_->GetFilteredHosts();
    EXPECT_EQ(expected_hosts, topology_hosts);
}

TEST_F(PluginServiceTest, InitClusterId_UserInput) {
    std::map<std::string, std::string> conn_info;
    std::string expected_id = "custom_id";
    conn_info.insert_or_assign(KEY_CLUSTER_ID, expected_id);
    std::string returned_id = PluginService::InitClusterId(conn_info);
    EXPECT_EQ(returned_id, expected_id);
}

TEST_F(PluginServiceTest, InitClusterId_Default) {
    std::map<std::string, std::string> conn_info;
    conn_info.insert_or_assign(KEY_SERVER, "NON_RDS");
    std::string returned_id = PluginService::InitClusterId(conn_info);
    EXPECT_EQ(DEFAULT_CLUSTER_ID, returned_id);
    EXPECT_EQ(DEFAULT_CLUSTER_ID, conn_info.at(KEY_CLUSTER_ID));
}

TEST_F(PluginServiceTest, InitClusterId_NotDerivedFromRdsHost) {
    // Cluster, reader, custom and proxy endpoints of different clusters all get the default
    for (const std::string& host : {
        "my-cluster.cluster-xyz.us-east-1.rds.amazonaws.com",
        "my-cluster.cluster-ro-xyz.us-west-2.rds.amazonaws.com",
        "my-custom.cluster-custom-xyz.us-east-1.rds.amazonaws.com",
        "my-proxy.proxy-xyz.us-east-1.rds.amazonaws.com",
        "instance-1.xyz.us-east-1.rds.amazonaws.com",
        "10.0.0.1"}) {
        std::map<std::string, std::string> conn_info = {{KEY_SERVER, host}};
        EXPECT_EQ(DEFAULT_CLUSTER_ID, PluginService::InitClusterId(conn_info)) << host;
    }
}

TEST_F(PluginServiceTest, InitClusterId_EmptyUsesDefault) {
    std::map<std::string, std::string> conn_info = {{KEY_CLUSTER_ID, ""}};
    EXPECT_EQ(DEFAULT_CLUSTER_ID, PluginService::InitClusterId(conn_info));
    EXPECT_EQ(DEFAULT_CLUSTER_ID, conn_info.at(KEY_CLUSTER_ID));
}

TEST_F(PluginServiceTest, HostFilter_IsPerEndpoint) {
    // Same cluster through its cluster endpoint, shares the topology but not the custom endpoint's filter
    const std::map<std::string, std::string> conn_attr = {
        {KEY_CLUSTER_ID, TEST_CLUSTER_ID},
        {KEY_SERVER, "plugin-service-test.cluster-xyz.us-east-1.rds.amazonaws.com"},
    };
    const auto cluster_endpoint_service = std::make_shared<PluginService>(nullptr, nullptr, conn_attr, "");

    HostFilter filter;
    filter.allowed_host_ids.insert(host_a_.GetHostId());
    plugin_service_->SetHostFilter(filter);

    EXPECT_EQ(std::vector<HostInfo>{host_a_}, plugin_service_->GetFilteredHosts());
    EXPECT_EQ(all_hosts_, cluster_endpoint_service->GetHosts());
    EXPECT_EQ(all_hosts_, cluster_endpoint_service->GetFilteredHosts());
}

TEST_F(PluginServiceTest, SetEndpointHostFilter) {
    HostFilter filter;
    filter.blocked_host_ids.insert(host_c_.GetHostId());
    PluginService::SetEndpointHostFilter(TEST_ENDPOINT, filter);

    const std::vector<HostInfo> expected_hosts = {host_a_, host_b_};
    EXPECT_EQ(expected_hosts, plugin_service_->GetFilteredHosts());

    // A connection opened later through the same endpoint sees the filter as well
    const std::map<std::string, std::string> conn_attr = {
        {KEY_CLUSTER_ID, TEST_CLUSTER_ID},
        {KEY_SERVER, TEST_ENDPOINT},
    };
    const auto later_service = std::make_shared<PluginService>(nullptr, nullptr, conn_attr, "");
    EXPECT_EQ(expected_hosts, later_service->GetFilteredHosts());
}
