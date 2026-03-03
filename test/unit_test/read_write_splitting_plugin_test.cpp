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

#include "rw_splitting_mock_objects.h"

#include "../../driver/plugin/read_write_splitting/read_write_splitting_plugin.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

class TestableReadWriteSplittingPlugin : public ReadWriteSplittingPlugin {
public:
    using ReadWriteSplittingPlugin::ReadWriteSplittingPlugin;
    void SetPluginHead(BasePlugin* head) { this->plugin_head_ = head; }
    void SetCurrentConnection(SQLHDBC conn) { this->current_connection_ = conn; }
};

class ReadWriteSplittingPluginTest : public ::testing::Test {
protected:
    static constexpr int TEST_PORT = 5432;

    HostInfo writer_host{"instance-0", TEST_PORT, UP, WRITER};
    HostInfo reader_host1{"instance-1", TEST_PORT, UP, READER};
    HostInfo reader_host2{"instance-2", TEST_PORT, UP, READER};
    HostInfo reader_host3{"instance-3", TEST_PORT, UP, READER};

    std::vector<HostInfo> default_hosts;
    std::vector<HostInfo> single_reader_topology;

    std::shared_ptr<NiceMock<MOCK_PLUGIN_SERVICE>> mock_plugin_service;
    std::shared_ptr<NiceMock<MOCK_ODBC_HELPER>> mock_odbc_helper;
    std::shared_ptr<NiceMock<MOCK_HOST_SELECTOR_GMOCK>> mock_host_selector;
    std::shared_ptr<NiceMock<MOCK_HOST_LIST_PROVIDER>> mock_hlp;
    std::shared_ptr<NiceMock<MOCK_DIALECT>> mock_dialect;
    std::shared_ptr<NiceMock<MOCK_TOPOLOGY_UTIL>> mock_topology_util;
    std::shared_ptr<RW_MockRdsLibLoader> mock_lib_loader;

    ENV env;
    DBC* dbc = nullptr;
    std::shared_ptr<MOCK_BASE_PLUGIN> mock_next_plugin;

    SQLHDBC fake_writer_hdbc = reinterpret_cast<SQLHDBC>(0x1000);
    SQLHDBC fake_reader_hdbc = reinterpret_cast<SQLHDBC>(0x2000);

    void SetUp() override {
        default_hosts = {writer_host, reader_host1, reader_host2, reader_host3};
        single_reader_topology = {writer_host, reader_host1};

        mock_lib_loader = std::make_shared<RW_MockRdsLibLoader>();
        mock_plugin_service = std::make_shared<NiceMock<MOCK_PLUGIN_SERVICE>>();
        mock_odbc_helper = std::make_shared<NiceMock<MOCK_ODBC_HELPER>>();
        mock_host_selector = std::make_shared<NiceMock<MOCK_HOST_SELECTOR_GMOCK>>();
        mock_hlp = std::make_shared<NiceMock<MOCK_HOST_LIST_PROVIDER>>();
        mock_dialect = std::make_shared<NiceMock<MOCK_DIALECT>>();
        mock_topology_util = std::make_shared<NiceMock<MOCK_TOPOLOGY_UTIL>>();

        env.driver_lib_loader = mock_lib_loader;

        dbc = new DBC();
        dbc->env = &env;
        dbc->wrapped_dbc = nullptr;
        dbc->transaction_status = TRANSACTION_CLOSED;
        dbc->plugin_service = mock_plugin_service;

        mock_next_plugin = std::make_shared<MOCK_BASE_PLUGIN>();
        dbc->plugin_head = mock_next_plugin.get();

        ON_CALL(*mock_plugin_service, GetOdbcHelper()).WillByDefault(Return(mock_odbc_helper));
        ON_CALL(*mock_plugin_service, GetHostSelector()).WillByDefault(Return(mock_host_selector));
        ON_CALL(*mock_plugin_service, GetHostListProvider()).WillByDefault(Return(mock_hlp));
        ON_CALL(*mock_plugin_service, GetDialect()).WillByDefault(Return(mock_dialect));
        ON_CALL(*mock_plugin_service, GetTopologyUtil()).WillByDefault(Return(mock_topology_util));
        ON_CALL(*mock_plugin_service, GetHosts()).WillByDefault(Return(default_hosts));
        ON_CALL(*mock_plugin_service, GetCurrentHostInfo()).WillByDefault(Return(writer_host));
        ON_CALL(*mock_plugin_service, GetOriginalConnAttr())
            .WillByDefault(Return(std::map<std::string, std::string>{}));

        ON_CALL(*mock_host_selector, GetHost(_, false, _)).WillByDefault(Return(reader_host1));
        ON_CALL(*mock_host_selector, GetHost(_, true, _)).WillByDefault(Return(writer_host));
        ON_CALL(*mock_dialect, GetDefaultPort()).WillByDefault(Return(TEST_PORT));
        ON_CALL(*mock_topology_util, GetWriter(_)).WillByDefault(Return(writer_host));
    }

    // Plugin takes ownership of mock_next_plugin via next_plugin chain.
    std::unique_ptr<TestableReadWriteSplittingPlugin> MakePlugin() {
        auto plugin = std::make_unique<TestableReadWriteSplittingPlugin>(dbc, mock_next_plugin);
        plugin->SetPluginHead(mock_next_plugin.get());
        return plugin;
    }

    void TearDown() override {
        if (dbc) {
            dbc->plugin_head = nullptr;
            delete dbc;
        }
    }
};

TEST_F(ReadWriteSplittingPluginTest, SwitchToReaderThenBackToWriter) {
    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_writer_hdbc;
    plugin->SetCurrentConnection(fake_writer_hdbc);

    DBC* reader_dbc = new DBC();
    reader_dbc->env = &env;
    reader_dbc->wrapped_dbc = fake_reader_hdbc;
    reader_dbc->plugin_head = nullptr;

    ON_CALL(*mock_odbc_helper, AllocDbc(_, _))
        .WillByDefault([reader_dbc](SQLHENV&, SQLHDBC& hdbc) -> SQLRETURN {
            hdbc = static_cast<SQLHDBC>(reader_dbc);
            return SQL_SUCCESS;
        });
    ON_CALL(*mock_next_plugin, Connect(_, _, _, _, _, _)).WillByDefault(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_plugin_service, SetCurrentHostInfo(_)).Times(testing::AtLeast(1));

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, writer_host);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc->wrapped_dbc, fake_reader_hdbc);

    DBC* writer_dbc = new DBC();
    writer_dbc->env = &env;
    writer_dbc->wrapped_dbc = fake_writer_hdbc;
    writer_dbc->plugin_head = nullptr;

    ON_CALL(*mock_odbc_helper, AllocDbc(_, _))
        .WillByDefault([writer_dbc](SQLHENV&, SQLHDBC& hdbc) -> SQLRETURN {
            hdbc = static_cast<SQLHDBC>(writer_dbc);
            return SQL_SUCCESS;
        });

    ret = plugin->SwitchConnectionIfRequired(false, reader_host1);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc->wrapped_dbc, fake_writer_hdbc);
}

TEST_F(ReadWriteSplittingPluginTest, AlreadyOnReader_NoSwitch) {
    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_reader_hdbc;
    plugin->SetCurrentConnection(fake_reader_hdbc);

    EXPECT_CALL(*mock_plugin_service, SetCurrentHostInfo(_)).Times(0);
    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, reader_host1);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc->wrapped_dbc, fake_reader_hdbc);
}

TEST_F(ReadWriteSplittingPluginTest, AlreadyOnWriter_NoSwitch) {
    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_writer_hdbc;
    plugin->SetCurrentConnection(fake_writer_hdbc);

    EXPECT_CALL(*mock_plugin_service, SetCurrentHostInfo(_)).Times(0);
    SQLRETURN ret = plugin->SwitchConnectionIfRequired(false, writer_host);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc->wrapped_dbc, fake_writer_hdbc);
}

TEST_F(ReadWriteSplittingPluginTest, SwitchToWriter_InTransaction_ReturnsError) {
    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_reader_hdbc;
    dbc->transaction_status = TRANSACTION_OPEN;
    plugin->SetCurrentConnection(fake_reader_hdbc);

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(false, reader_host1);
    EXPECT_EQ(ret, SQL_ERROR);
}

TEST_F(ReadWriteSplittingPluginTest, SingleHost_FallsBackToWriter) {
    ON_CALL(*mock_plugin_service, GetHosts())
        .WillByDefault(Return(std::vector<HostInfo>{writer_host}));

    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_writer_hdbc;
    plugin->SetCurrentConnection(fake_writer_hdbc);

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, writer_host);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc->wrapped_dbc, fake_writer_hdbc);
}

TEST_F(ReadWriteSplittingPluginTest, ShouldUpdateWriterConnection) {
    auto plugin = MakePlugin();
    EXPECT_TRUE(plugin->ShouldUpdateWriterConnection(writer_host));
    EXPECT_FALSE(plugin->ShouldUpdateWriterConnection(reader_host1));
}

TEST_F(ReadWriteSplittingPluginTest, ShouldUpdateReaderConnection) {
    auto plugin = MakePlugin();
    EXPECT_TRUE(plugin->ShouldUpdateReaderConnection(reader_host1));
    EXPECT_FALSE(plugin->ShouldUpdateReaderConnection(writer_host));
}

TEST_F(ReadWriteSplittingPluginTest, RefreshAndStoreTopology_Success) {
    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_writer_hdbc;
    plugin->SetCurrentConnection(fake_writer_hdbc);

    SQLRETURN ret = plugin->RefreshAndStoreTopology();
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
}

TEST_F(ReadWriteSplittingPluginTest, RefreshAndStoreTopology_EmptyHosts_ReturnsError) {
    ON_CALL(*mock_plugin_service, GetHosts())
        .WillByDefault(Return(std::vector<HostInfo>{}));

    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_writer_hdbc;
    plugin->SetCurrentConnection(fake_writer_hdbc);

    SQLRETURN ret = plugin->RefreshAndStoreTopology();
    EXPECT_EQ(ret, SQL_ERROR);
}
