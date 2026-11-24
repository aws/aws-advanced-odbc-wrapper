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

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "../../driver/plugin/blue_green/blue_green_interim_status.h"
#include "../../driver/plugin/blue_green/blue_green_phase.h"
#include "../../driver/plugin/blue_green/blue_green_role.h"
#include "../../driver/plugin/blue_green/blue_green_status.h"
#include "../../driver/plugin/blue_green/blue_green_status_provider.h"
#include "../../driver/util/concurrent_map.h"

namespace {
    const std::string BG_ID = "bgd-unit-test";
    const int PORT = 5432;

    const std::string BLUE_HOST = "db-1.xyz.us-west-2.rds.amazonaws.com";
    const std::string BLUE_CLUSTER_HOST = "db.cluster-xyz.us-west-2.rds.amazonaws.com";
    const std::string BLUE_IP = "10.0.0.1";
    const std::string GREEN_HOST = "db-1-green-abc123.xyz.us-west-2.rds.amazonaws.com";
    const std::string GREEN_CLUSTER_HOST = "db-green-abc123.cluster-xyz.us-west-2.rds.amazonaws.com";
    const std::string GREEN_IP = "10.0.0.2";

    // Exposes PrepareStatus, which the monitors call, so tests can replay a sequence of interim statuses.
    class TestBlueGreenStatusProvider : public BlueGreenStatusProvider {
    public:
        using BlueGreenStatusProvider::BlueGreenStatusProvider;
        using BlueGreenStatusProvider::PrepareStatus;
    };

    BlueGreenInterimStatus MakeStatus(BlueGreenPhase phase, const std::string& writer_host,
                                      const std::string& cluster_host, const std::string& ip) {
        const std::vector<HostInfo> topology{HostInfo(writer_host, PORT, UP, WRITER)};
        const std::map<std::string, std::optional<std::string>> ips{{writer_host, ip}, {cluster_host, ip}};
        return {phase, "1.0", PORT, topology, topology, ips, ips, {writer_host, cluster_host}, false, false, false};
    }
}  // namespace

class BlueGreenStatusProviderTest : public testing::Test {
protected:
    std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> status_cache_ =
        std::make_shared<ConcurrentMap<std::string, BlueGreenStatus>>();
    TestBlueGreenStatusProvider provider_{nullptr, {}, status_cache_, BG_ID, "db"};

    BlueGreenStatus CachedStatus() {
        const std::optional<BlueGreenStatus> status = status_cache_->Get(BG_ID);
        EXPECT_TRUE(status.has_value());
        return status.value_or(BlueGreenStatus{});
    }

    static bool HasSubstituteRouteFor(BlueGreenStatus status, const std::string& host, BlueGreenRole role) {
        for (const auto& route : status.GetConnectRoutes()) {
            if (route->IsMatch(host, role) && route->ToString().find("SubstituteConnectRouting") != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

TEST_F(BlueGreenStatusProviderTest, PostRoutesBlueToGreen) {
    provider_.PrepareStatus(BlueGreenRole::SOURCE, MakeStatus(BlueGreenPhase::POST, BLUE_HOST, BLUE_CLUSTER_HOST, BLUE_IP));
    provider_.PrepareStatus(BlueGreenRole::TARGET, MakeStatus(BlueGreenPhase::POST, GREEN_HOST, GREEN_CLUSTER_HOST, GREEN_IP));

    BlueGreenStatus status = CachedStatus();
    EXPECT_EQ(BlueGreenPhase::POST, status.GetCurrentPhase().GetPhase());
    const auto nodes = status.GetCorrespondingNodes();
    ASSERT_TRUE(nodes.contains(BLUE_HOST));
    EXPECT_EQ(GREEN_HOST, nodes.at(BLUE_HOST).second.GetHost());
    EXPECT_TRUE(HasSubstituteRouteFor(status, BLUE_HOST, BlueGreenRole::SOURCE));
}

// The blue monitor reports phase UNKNOWN once its connection to the old blue drops mid-switchover. Its start topology
// is still valid, so blue connections must keep being rerouted to green instead of falling back to DNS (old blue).
TEST_F(BlueGreenStatusProviderTest, PostKeepsRoutingWhenBlueMonitorLosesConnection) {
    provider_.PrepareStatus(BlueGreenRole::SOURCE, MakeStatus(BlueGreenPhase::POST, BLUE_HOST, BLUE_CLUSTER_HOST, BLUE_IP));
    provider_.PrepareStatus(BlueGreenRole::TARGET, MakeStatus(BlueGreenPhase::POST, GREEN_HOST, GREEN_CLUSTER_HOST, GREEN_IP));
    provider_.PrepareStatus(BlueGreenRole::SOURCE, MakeStatus(BlueGreenPhase::UNKNOWN, BLUE_HOST, BLUE_CLUSTER_HOST, BLUE_IP));

    BlueGreenStatus status = CachedStatus();
    EXPECT_EQ(BlueGreenPhase::POST, status.GetCurrentPhase().GetPhase());
    const auto nodes = status.GetCorrespondingNodes();
    ASSERT_TRUE(nodes.contains(BLUE_HOST));
    EXPECT_EQ(GREEN_HOST, nodes.at(BLUE_HOST).second.GetHost());
    EXPECT_TRUE(nodes.contains(BLUE_CLUSTER_HOST));
    EXPECT_TRUE(HasSubstituteRouteFor(status, BLUE_HOST, BlueGreenRole::SOURCE));
}

// Before any monitor has reported for a role there is nothing to map.
TEST_F(BlueGreenStatusProviderTest, NoMappingUntilBothRolesReport) {
    provider_.PrepareStatus(BlueGreenRole::SOURCE, MakeStatus(BlueGreenPhase::POST, BLUE_HOST, BLUE_CLUSTER_HOST, BLUE_IP));

    EXPECT_TRUE(CachedStatus().GetCorrespondingNodes().empty());
}
