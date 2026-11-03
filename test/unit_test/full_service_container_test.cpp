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

#include <memory>
#include <stdexcept>
#include <string>

#include "../../driver/driver.h"
#include "../../driver/plugin/base_plugin.h"
#include "../../driver/plugin/default_plugin.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/util/core_service.h"
#include "../../driver/util/full_service_container.h"
#include "../../driver/util/plugin_manager.h"
#include "../../driver/util/plugin_service.h"

using testing::Return;

namespace {
    class MockChainPlugin : public BasePlugin {
    public:
        MockChainPlugin() : BasePlugin() {}

        MOCK_METHOD(SQLRETURN, Connect,
            (SQLHDBC ConnectionHandle, SQLHWND WindowHandle, SQLTCHAR *OutConnectionString, SQLSMALLINT BufferLength,
            SQLSMALLINT *StringLengthPtr, SQLUSMALLINT DriverCompletion), (override));
        MOCK_METHOD(SQLRETURN, Execute,
            (SQLHSTMT StatementHandle, SQLTCHAR* StatementText, SQLINTEGER TextLength), (override));
        MOCK_METHOD(void, ReleaseResources, (), (override));
    };
}  // namespace

class FullServiceContainerTest : public testing::Test {
protected:
    ENV env_;
    DBC* dbc_ = nullptr;

    void SetUp() override {
        dbc_ = new DBC();
        dbc_->env = &env_;
        dbc_->conn_attr.insert_or_assign(KEY_SERVER, "database-test-name.cluster-XYZ.us-east-2.rds.amazonaws.com");
    }

    void TearDown() override {
        delete dbc_;
    }
};

TEST_F(FullServiceContainerTest, NewDbcHasEmptyContainer) {
    ASSERT_NE(nullptr, dbc_->service_container);
    EXPECT_EQ(nullptr, dbc_->service_container->GetPluginService());
    EXPECT_EQ(nullptr, dbc_->service_container->GetPluginManager());
}

TEST_F(FullServiceContainerTest, CoreServicesRetrievedFromCore) {
    CoreService& core_service = CoreService::Instance();
    const FullServiceContainer container;
    EXPECT_EQ(&core_service.GetStorageService(), &container.GetStorageService());
    EXPECT_EQ(&core_service.GetMonitorService(), &container.GetMonitorService());
    // Every DBC's container shares the same services
    EXPECT_EQ(&container.GetStorageService(), &dbc_->service_container->GetStorageService());
    EXPECT_EQ(&container.GetMonitorService(), &dbc_->service_container->GetMonitorService());
}

TEST_F(FullServiceContainerTest, InitConnectionServices_CreatesServices) {
    FullServiceContainer::InitConnectionServices(dbc_, "");

    const std::shared_ptr<PluginService> plugin_service = dbc_->service_container->GetPluginService();
    const std::shared_ptr<PluginManager> plugin_manager = dbc_->service_container->GetPluginManager();
    ASSERT_NE(nullptr, plugin_service);
    ASSERT_NE(nullptr, plugin_manager);
    // Plain connections only get the default plugin
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<DefaultPlugin>(plugin_manager->GetPluginChain()));
    // Cluster ID is generated into the connection attributes before the PluginService is created
    EXPECT_TRUE(dbc_->conn_attr.contains(KEY_CLUSTER_ID));
    EXPECT_EQ(dbc_->conn_attr.at(KEY_CLUSTER_ID), plugin_service->GetClusterId());
}

TEST_F(FullServiceContainerTest, InitConnectionServices_ReusedOnReconnect) {
    FullServiceContainer::InitConnectionServices(dbc_, "");
    const std::shared_ptr<PluginService> plugin_service = dbc_->service_container->GetPluginService();
    const std::shared_ptr<PluginManager> plugin_manager = dbc_->service_container->GetPluginManager();

    // A reconnect on the same DBC keeps the services of the first connect
    dbc_->conn_attr.insert_or_assign(KEY_SERVER, "other-host.cluster-ABC.us-west-2.rds.amazonaws.com");
    FullServiceContainer::InitConnectionServices(dbc_, "");

    EXPECT_EQ(plugin_service, dbc_->service_container->GetPluginService());
    EXPECT_EQ(plugin_manager, dbc_->service_container->GetPluginManager());
}

TEST_F(FullServiceContainerTest, InitConnectionServices_FailedChainIsRetried) {
    // Simple read write splitting without its endpoints
    dbc_->conn_attr.insert_or_assign(KEY_ENABLE_SRW_SPLIT, VALUE_BOOL_TRUE);
    EXPECT_THROW(FullServiceContainer::InitConnectionServices(dbc_, ""), std::runtime_error);
    // The PluginService exists, but a chain that failed to build is never published
    EXPECT_NE(nullptr, dbc_->service_container->GetPluginService());
    EXPECT_EQ(nullptr, dbc_->service_container->GetPluginManager());

    dbc_->conn_attr.erase(KEY_ENABLE_SRW_SPLIT);
    FullServiceContainer::InitConnectionServices(dbc_, "");
    EXPECT_NE(nullptr, dbc_->service_container->GetPluginManager());
}

TEST_F(FullServiceContainerTest, InitMonitoringServices_CreatesReducedServices) {
    DBC monitor_dbc;
    monitor_dbc.conn_attr = dbc_->conn_attr;
    FullServiceContainer::InitMonitoringServices(&monitor_dbc, nullptr);

    ASSERT_NE(nullptr, monitor_dbc.service_container->GetPluginService());
    const std::shared_ptr<PluginManager> plugin_manager = monitor_dbc.service_container->GetPluginManager();
    ASSERT_NE(nullptr, plugin_manager);
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<DefaultPlugin>(plugin_manager->GetPluginChain()));
}

TEST_F(FullServiceContainerTest, BorrowedPluginServiceHasNoPluginManager) {
    FullServiceContainer::InitConnectionServices(dbc_, "");

    // As read write splitting and the monitors do for their internal connections
    DBC internal_dbc;
    internal_dbc.service_container->SetPluginService(dbc_->service_container->GetPluginService());

    EXPECT_EQ(dbc_->service_container->GetPluginService(), internal_dbc.service_container->GetPluginService());
    EXPECT_EQ(nullptr, internal_dbc.service_container->GetPluginManager());
}

TEST(PluginManagerTest, DispatchesToChain) {
    const std::shared_ptr<MockChainPlugin> plugin = std::make_shared<MockChainPlugin>();
    const PluginManager plugin_manager(plugin);
    EXPECT_EQ(plugin, plugin_manager.GetPluginChain());

    EXPECT_CALL(*plugin, Connect(nullptr, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT)).WillOnce(Return(SQL_SUCCESS));
    EXPECT_EQ(SQL_SUCCESS, plugin_manager.Connect(nullptr, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));

    EXPECT_CALL(*plugin, Execute(nullptr, nullptr, -1)).WillOnce(Return(SQL_ERROR));
    EXPECT_EQ(SQL_ERROR, plugin_manager.Execute(nullptr));

    EXPECT_CALL(*plugin, ReleaseResources()).Times(1);
    plugin_manager.ReleaseResources();
}

TEST(PluginManagerTest, ReleaseResourcesWithoutChain) {
    const PluginManager plugin_manager;
    EXPECT_EQ(nullptr, plugin_manager.GetPluginChain());
    EXPECT_NO_THROW(plugin_manager.ReleaseResources());
}
