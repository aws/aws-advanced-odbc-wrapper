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

#include "auth_mock_objects.h"
#include "limitless_mock_objects.h"

#include "../../driver/driver.h"
#include "../../driver/dialect/dialect_aurora_postgres.h"
#include "../../driver/plugin/limitless/limitless_router_service.h"
#include "../../driver/util/connection_string_keys.h"

class LimitlessRouterServiceTest : public testing::Test {
protected:
    MOCK_BASE_PLUGIN* mock_base_plugin;
    DBC* dbc;
    std::shared_ptr<OdbcHelper> odbc_helper_;

    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    // Runs per test
    void SetUp() override {
        mock_base_plugin = new MOCK_BASE_PLUGIN();
        dbc = new DBC();
        dbc->plugin_head = mock_base_plugin;
        odbc_helper_ = std::make_shared<OdbcHelper>(nullptr);
    }
    void TearDown() override {
        // mock_base_plugin should be cleaned up by plugin chain
        if (dbc) delete dbc;
    }
};

TEST_F(LimitlessRouterServiceTest, LimitlessRouterMonitorReferenceCountingTest) {
    const std::map<std::string, std::string> attr1 = {
        {KEY_SERVER, "server_1"}
    };
    const std::map<std::string, std::string> attr2 = {
        {KEY_SERVER, "server_2"}
    };

    std::shared_ptr<DialectLimitless> dialect = std::make_shared<DialectAuroraPostgresLimitless>();

    dbc->conn_attr = attr1;
    TestLimitlessRouterService* router_service1 = new TestLimitlessRouterService(dialect, attr1, odbc_helper_);
    TestLimitlessRouterService* router_service2 = new TestLimitlessRouterService(dialect, attr1, odbc_helper_);
    router_service1->StartMonitoring(dbc, dialect);
    router_service2->StartMonitoring(dbc, dialect);

    dbc->conn_attr = attr2;
    TestLimitlessRouterService* router_service3 = new TestLimitlessRouterService(dialect, attr2, odbc_helper_);
    router_service3->StartMonitoring(dbc, dialect);

    std::pair<unsigned int, std::shared_ptr<LimitlessRouterMonitor>> pair = LimitlessRouterService::limitless_router_monitors.Get("server_1");
    EXPECT_EQ(2, pair.first);
    pair = LimitlessRouterService::limitless_router_monitors.Get("server_2");
    EXPECT_EQ(1, pair.first);

    delete router_service1;
    pair = LimitlessRouterService::limitless_router_monitors.Get("server_1");
    EXPECT_EQ(1, pair.first);
    pair = LimitlessRouterService::limitless_router_monitors.Get("server_2");
    EXPECT_EQ(1, pair.first);

    delete router_service2;
    pair = LimitlessRouterService::limitless_router_monitors.Get("server_1");
    EXPECT_EQ(0, pair.first);
    pair = LimitlessRouterService::limitless_router_monitors.Get("server_2");
    EXPECT_EQ(1, pair.first);

    delete router_service3;
    pair = LimitlessRouterService::limitless_router_monitors.Get("server_2");
    EXPECT_EQ(0, pair.first);
}
