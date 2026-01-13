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

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "auth_mock_objects.h"
#include "custom_endpoint_mocks.h"

#include "../../driver/plugin/custom_endpoint/custom_endpoint_plugin.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/driver.h"

class CustomEndpointPluginTest : public testing::Test {
protected:
    MOCK_BASE_PLUGIN* mock_base_plugin;
    std::shared_ptr<MOCK_CUSTOM_ENDPOINT_MONITOR> mock_custom_endpoint_monitor;
    DBC* dbc;

    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    void SetUp() override {
        mock_base_plugin = new MOCK_BASE_PLUGIN();
        mock_custom_endpoint_monitor = std::make_shared<MOCK_CUSTOM_ENDPOINT_MONITOR>();
        dbc = new DBC();
        dbc->conn_attr.insert_or_assign(KEY_CLUSTER_ID, "cluster_id");
        dbc->conn_attr.insert_or_assign(KEY_SERVER, "cluster-name.cluster-1234id.region.rds.amazonaws.com");
        dbc->conn_attr.insert_or_assign(KEY_PORT, "port");
    }

    void TearDown() override {
        if (dbc) delete dbc;
    }
};

TEST_F(CustomEndpointPluginTest, Connect_RdsDsn) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(
        *mock_custom_endpoint_monitor,
        HasInfo())
        .Times(testing::Exactly(0));

    CustomEndpointPlugin plugin(dbc, mock_base_plugin, nullptr, mock_custom_endpoint_monitor);
    plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
}

TEST_F(CustomEndpointPluginTest, Connect_RdsDsn_WaitForInfo) {
    dbc->conn_attr.insert_or_assign(KEY_WAIT_FOR_CUSTOM_ENDPOINT_INFO, VALUE_BOOL_TRUE);

    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(
        *mock_custom_endpoint_monitor,
        HasInfo())
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(true));

    CustomEndpointPlugin plugin(dbc, mock_base_plugin, nullptr, mock_custom_endpoint_monitor);
    plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
}

TEST_F(CustomEndpointPluginTest, Connect_RdsDsn_WaitForInfo_Timeout) {
    std::string sleep_duration_ms = "5000";
    dbc->conn_attr.insert_or_assign(KEY_WAIT_FOR_CUSTOM_ENDPOINT_INFO, VALUE_BOOL_TRUE);
    dbc->conn_attr.insert_or_assign(KEY_WAIT_FOR_CUSTOM_ENDPOINT_INFO_TIMEOUT_MS, sleep_duration_ms);

    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(
        *mock_custom_endpoint_monitor,
        HasInfo())
        .WillRepeatedly(testing::Return(false));

    const auto wait_for_end = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(std::strtol(sleep_duration_ms.c_str(), nullptr, 0))
        - CustomEndpointPlugin::WAIT_FOR_INFO_SLEEP_DIR_MS; // Leeway
    CustomEndpointPlugin plugin(dbc, mock_base_plugin, nullptr, mock_custom_endpoint_monitor);
    plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_GT(std::chrono::steady_clock::now(), wait_for_end);
}

TEST_F(CustomEndpointPluginTest, Connect_NonRdsDsn) {
    dbc->conn_attr.insert_or_assign(KEY_SERVER, "instance-endpoint");
    dbc->conn_attr.insert_or_assign(KEY_CUSTOM_ENDPOINT_REGION, "us-west-1");

    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(
        *mock_custom_endpoint_monitor,
        HasInfo())
        .Times(testing::Exactly(0));

    CustomEndpointPlugin plugin(dbc, mock_base_plugin, nullptr, mock_custom_endpoint_monitor);
    plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
}

TEST_F(CustomEndpointPluginTest, Connect_NonRdsDsn_NoRegion) {
    dbc->conn_attr.insert_or_assign(KEY_SERVER, "non-rds-host");

    EXPECT_THROW({
        try {
            CustomEndpointPlugin* plugin = new CustomEndpointPlugin(dbc, mock_base_plugin, nullptr, mock_custom_endpoint_monitor);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ("Unable to determine connection region. If you are using a non-standard RDS URL, please set the 'CUSTOM_ENDPOINT_REGION' property", e.what());
            throw e;
        }
    }, std::runtime_error);
}
