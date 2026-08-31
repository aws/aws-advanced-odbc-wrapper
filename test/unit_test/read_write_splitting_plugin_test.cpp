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

    HostInfo writer_host_{"instance-0", TEST_PORT, UP, WRITER};
    HostInfo reader_host1_{"instance-1", TEST_PORT, UP, READER};
    HostInfo reader_host2_{"instance-2", TEST_PORT, UP, READER};
    HostInfo reader_host3_{"instance-3", TEST_PORT, UP, READER};

    std::vector<HostInfo> default_hosts_;
    std::vector<HostInfo> single_reader_topology_;

    std::shared_ptr<NiceMock<MockPluginService>> mock_plugin_service_;
    std::shared_ptr<NiceMock<MockOdbcHelper>> mock_odbc_helper_;
    std::shared_ptr<NiceMock<MockHostSelector>> mock_host_selector_;
    std::shared_ptr<NiceMock<MockHostListProvider>> mock_hlp_;
    std::shared_ptr<NiceMock<MockDialect>> mock_dialect_;
    std::shared_ptr<NiceMock<MockTopologyUtil>> mock_topology_util_;
    std::shared_ptr<RwMockRdsLibLoader> mock_lib_loader_;

    ENV env_;
    DBC* dbc_ = nullptr;
    std::shared_ptr<NiceMock<MockBasePlugin>> mock_next_plugin_;

    SQLHDBC fake_writer_hdbc_ = reinterpret_cast<SQLHDBC>(0x1000);
    SQLHDBC fake_reader_hdbc_ = reinterpret_cast<SQLHDBC>(0x2000);

    void SetUp() override {
        default_hosts_ = {writer_host_, reader_host1_, reader_host2_, reader_host3_};
        single_reader_topology_ = {writer_host_, reader_host1_};

        mock_lib_loader_ = std::make_shared<RwMockRdsLibLoader>();
        mock_plugin_service_ = std::make_shared<NiceMock<MockPluginService>>();
        mock_odbc_helper_ = std::make_shared<NiceMock<MockOdbcHelper>>();
        mock_host_selector_ = std::make_shared<NiceMock<MockHostSelector>>();
        mock_hlp_ = std::make_shared<NiceMock<MockHostListProvider>>();
        mock_dialect_ = std::make_shared<NiceMock<MockDialect>>();
        mock_topology_util_ = std::make_shared<NiceMock<MockTopologyUtil>>();

        env_.driver_lib_loader = mock_lib_loader_;

        dbc_ = new DBC();
        dbc_->env = &env_;
        dbc_->wrapped_dbc = nullptr;
        dbc_->transaction_status = TRANSACTION_CLOSED;
        dbc_->plugin_service = mock_plugin_service_;

        mock_next_plugin_ = std::make_shared<NiceMock<MockBasePlugin>>();
        dbc_->plugin_head = mock_next_plugin_.get();

        ON_CALL(*mock_plugin_service_, GetOdbcHelper()).WillByDefault(Return(mock_odbc_helper_));
        ON_CALL(*mock_plugin_service_, GetHostSelector()).WillByDefault(Return(mock_host_selector_));
        ON_CALL(*mock_plugin_service_, GetHostListProvider()).WillByDefault(Return(mock_hlp_));
        ON_CALL(*mock_plugin_service_, GetDialect()).WillByDefault(Return(mock_dialect_));
        ON_CALL(*mock_plugin_service_, GetTopologyUtil()).WillByDefault(Return(mock_topology_util_));
        ON_CALL(*mock_plugin_service_, GetHosts()).WillByDefault(Return(default_hosts_));
        ON_CALL(*mock_plugin_service_, GetCurrentHostInfo()).WillByDefault(Return(writer_host_));
        ON_CALL(*mock_plugin_service_, GetOriginalConnAttr())
            .WillByDefault(Return(std::map<std::string, std::string>{}));

        ON_CALL(*mock_host_selector_, GetHost(_, false, _)).WillByDefault(Return(reader_host1_));
        ON_CALL(*mock_host_selector_, GetHost(_, true, _)).WillByDefault(Return(writer_host_));
        ON_CALL(*mock_dialect_, GetDefaultPort()).WillByDefault(Return(TEST_PORT));
        ON_CALL(*mock_topology_util_, GetWriter(_)).WillByDefault(Return(writer_host_));
    }

    // Plugin takes ownership of mock_next_plugin via next_plugin chain.
    std::unique_ptr<TestableReadWriteSplittingPlugin> MakePlugin() {
        auto plugin = std::make_unique<TestableReadWriteSplittingPlugin>(dbc_, mock_next_plugin_);
        plugin->SetPluginHead(mock_next_plugin_.get());
        return plugin;
    }

    void TearDown() override {
        if (dbc_) {
            dbc_->plugin_head = nullptr;
            delete dbc_;
        }
    }
};

TEST_F(ReadWriteSplittingPluginTest, SwitchToReaderThenBackToWriter) {
    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_writer_hdbc_;
    plugin->SetCurrentConnection(fake_writer_hdbc_);

    DBC* reader_dbc = new DBC();
    reader_dbc->env = &env_;
    reader_dbc->wrapped_dbc = fake_reader_hdbc_;
    reader_dbc->plugin_head = nullptr;

    ON_CALL(*mock_odbc_helper_, AllocDbc(_, _))
        .WillByDefault([reader_dbc](SQLHENV&, SQLHDBC& hdbc) -> SQLRETURN {
            hdbc = static_cast<SQLHDBC>(reader_dbc);
            return SQL_SUCCESS;
        });
    ON_CALL(*mock_next_plugin_, Connect(_, _, _, _, _, _)).WillByDefault(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_plugin_service_, SetCurrentHostInfo(_)).Times(testing::AtLeast(1));

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, writer_host_);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc_->wrapped_dbc, fake_reader_hdbc_);

    DBC* writer_dbc = new DBC();
    writer_dbc->env = &env_;
    writer_dbc->wrapped_dbc = fake_writer_hdbc_;
    writer_dbc->plugin_head = nullptr;

    ON_CALL(*mock_odbc_helper_, AllocDbc(_, _))
        .WillByDefault([writer_dbc](SQLHENV&, SQLHDBC& hdbc) -> SQLRETURN {
            hdbc = static_cast<SQLHDBC>(writer_dbc);
            return SQL_SUCCESS;
        });

    ret = plugin->SwitchConnectionIfRequired(false, reader_host1_);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc_->wrapped_dbc, fake_writer_hdbc_);
}

TEST_F(ReadWriteSplittingPluginTest, AlreadyOnReader_NoSwitch) {
    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_reader_hdbc_;
    plugin->SetCurrentConnection(fake_reader_hdbc_);

    EXPECT_CALL(*mock_plugin_service_, SetCurrentHostInfo(_)).Times(0);
    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, reader_host1_);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc_->wrapped_dbc, fake_reader_hdbc_);
}

TEST_F(ReadWriteSplittingPluginTest, AlreadyOnWriter_NoSwitch) {
    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_writer_hdbc_;
    plugin->SetCurrentConnection(fake_writer_hdbc_);

    EXPECT_CALL(*mock_plugin_service_, SetCurrentHostInfo(_)).Times(0);
    SQLRETURN ret = plugin->SwitchConnectionIfRequired(false, writer_host_);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc_->wrapped_dbc, fake_writer_hdbc_);
}

TEST_F(ReadWriteSplittingPluginTest, SwitchToWriter_InTransaction_ReturnsError) {
    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_reader_hdbc_;
    dbc_->transaction_status = TRANSACTION_OPEN;
    plugin->SetCurrentConnection(fake_reader_hdbc_);

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(false, reader_host1_);
    EXPECT_EQ(ret, SQL_ERROR);
}

TEST_F(ReadWriteSplittingPluginTest, SingleHost_FallsBackToWriter) {
    ON_CALL(*mock_plugin_service_, GetHosts())
        .WillByDefault(Return(std::vector<HostInfo>{writer_host_}));

    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_writer_hdbc_;
    plugin->SetCurrentConnection(fake_writer_hdbc_);

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, writer_host_);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc_->wrapped_dbc, fake_writer_hdbc_);
}

TEST_F(ReadWriteSplittingPluginTest, ShouldUpdateWriterConnection) {
    auto plugin = MakePlugin();
    EXPECT_TRUE(plugin->ShouldUpdateWriterConnection(writer_host_));
    EXPECT_FALSE(plugin->ShouldUpdateWriterConnection(reader_host1_));
}

TEST_F(ReadWriteSplittingPluginTest, ShouldUpdateReaderConnection) {
    auto plugin = MakePlugin();
    EXPECT_TRUE(plugin->ShouldUpdateReaderConnection(reader_host1_));
    EXPECT_FALSE(plugin->ShouldUpdateReaderConnection(writer_host_));
}

TEST_F(ReadWriteSplittingPluginTest, RefreshAndStoreTopology_Success) {
    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_writer_hdbc_;
    plugin->SetCurrentConnection(fake_writer_hdbc_);

    SQLRETURN ret = plugin->RefreshAndStoreTopology();
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
}

TEST_F(ReadWriteSplittingPluginTest, RefreshAndStoreTopology_EmptyHosts_ReturnsError) {
    ON_CALL(*mock_plugin_service_, GetHosts())
        .WillByDefault(Return(std::vector<HostInfo>{}));

    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_writer_hdbc_;
    plugin->SetCurrentConnection(fake_writer_hdbc_);

    SQLRETURN ret = plugin->RefreshAndStoreTopology();
    EXPECT_EQ(ret, SQL_ERROR);
}
