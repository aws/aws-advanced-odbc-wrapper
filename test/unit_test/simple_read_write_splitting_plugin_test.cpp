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

#include "../../driver/plugin/read_write_splitting/simple_read_write_splitting_plugin.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

class TestableSimpleReadWriteSplittingPlugin : public SimpleReadWriteSplittingPlugin {
public:
    using SimpleReadWriteSplittingPlugin::SimpleReadWriteSplittingPlugin;
    void SetPluginHead(BasePlugin* head) { this->plugin_head_ = head; }
    void SetCurrentConnection(SQLHDBC conn) { this->current_connection_ = conn; }
};

class SimpleReadWriteSplittingPluginTest : public ::testing::Test {
protected:
    static constexpr int TEST_PORT = 5432;
    static constexpr const char* WRITE_ENDPOINT = "writer.cluster-xyz.us-east-1.rds.amazonaws.com";
    static constexpr const char* READ_ENDPOINT  = "reader.cluster-xyz.us-east-1.rds.amazonaws.com";

    HostInfo writer_host_{WRITE_ENDPOINT, TEST_PORT, UP, WRITER};
    HostInfo reader_host_{READ_ENDPOINT, TEST_PORT, UP, READER};

    std::shared_ptr<NiceMock<MockPluginService>> mock_plugin_service_;
    std::shared_ptr<NiceMock<MockOdbcHelper>> mock_odbc_helper_;
    std::shared_ptr<NiceMock<MockHostListProvider>> mock_hlp_;
    std::shared_ptr<NiceMock<MockDialect>> mock_dialect_;
    std::shared_ptr<RwMockRdsLibLoader> mock_lib_loader_;

    ENV env_;
    DBC* dbc_ = nullptr;
    std::shared_ptr<NiceMock<MockBasePlugin>> mock_next_plugin_;

    SQLHDBC fake_writer_hdbc_ = reinterpret_cast<SQLHDBC>(0x3000);
    SQLHDBC fake_reader_hdbc_ = reinterpret_cast<SQLHDBC>(0x4000);

    void SetUp() override {
        mock_lib_loader_ = std::make_shared<RwMockRdsLibLoader>();
        mock_plugin_service_ = std::make_shared<NiceMock<MockPluginService>>();
        mock_odbc_helper_ = std::make_shared<NiceMock<MockOdbcHelper>>();
        mock_hlp_ = std::make_shared<NiceMock<MockHostListProvider>>();
        mock_dialect_ = std::make_shared<NiceMock<MockDialect>>();

        env_.driver_lib_loader = mock_lib_loader_;

        dbc_ = new DBC();
        dbc_->env = &env_;
        dbc_->wrapped_dbc = nullptr;
        dbc_->transaction_status = TRANSACTION_CLOSED;
        dbc_->plugin_service = mock_plugin_service_;

        mock_next_plugin_ = std::make_shared<NiceMock<MockBasePlugin>>();
        dbc_->plugin_head = mock_next_plugin_.get();

        dbc_->conn_attr[KEY_SRW_WRITE_ENDPOINT] = WRITE_ENDPOINT;
        dbc_->conn_attr[KEY_SRW_READ_ENDPOINT] = READ_ENDPOINT;

        ON_CALL(*mock_plugin_service_, GetOdbcHelper()).WillByDefault(Return(mock_odbc_helper_));
        ON_CALL(*mock_plugin_service_, GetHostListProvider()).WillByDefault(Return(mock_hlp_));
        ON_CALL(*mock_plugin_service_, GetDialect()).WillByDefault(Return(mock_dialect_));
        ON_CALL(*mock_plugin_service_, GetHosts()).WillByDefault(Return(std::vector<HostInfo>{}));
        ON_CALL(*mock_plugin_service_, GetCurrentHostInfo()).WillByDefault(Return(writer_host_));
        ON_CALL(*mock_plugin_service_, GetOriginalConnAttr())
            .WillByDefault(Return(dbc_->conn_attr));

        ON_CALL(*mock_dialect_, GetDefaultPort()).WillByDefault(Return(TEST_PORT));
        ON_CALL(*mock_hlp_, GetConnectionRole(_)).WillByDefault(Return(UNKNOWN));
    }

    void TearDown() override {
        if (dbc_) {
            dbc_->plugin_head = nullptr;
            delete dbc_;
        }
    }

    std::unique_ptr<TestableSimpleReadWriteSplittingPlugin> MakePlugin() {
        auto plugin = std::make_unique<TestableSimpleReadWriteSplittingPlugin>(dbc_, mock_next_plugin_);
        plugin->SetPluginHead(mock_next_plugin_.get());
        return plugin;
    }
};

// ---------------------------------------------------------------------------
// Constructor tests (pass nullptr — no next_plugin needed)
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, Constructor_MissingWriteEndpoint_Throws) {
    dbc_->conn_attr.erase(KEY_SRW_WRITE_ENDPOINT);
    EXPECT_THROW(SimpleReadWriteSplittingPlugin(dbc_, nullptr), std::runtime_error);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Constructor_MissingReadEndpoint_Throws) {
    dbc_->conn_attr.erase(KEY_SRW_READ_ENDPOINT);
    EXPECT_THROW(SimpleReadWriteSplittingPlugin(dbc_, nullptr), std::runtime_error);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Constructor_MissingBothEndpoints_Throws) {
    dbc_->conn_attr.erase(KEY_SRW_WRITE_ENDPOINT);
    dbc_->conn_attr.erase(KEY_SRW_READ_ENDPOINT);
    EXPECT_THROW(SimpleReadWriteSplittingPlugin(dbc_, nullptr), std::runtime_error);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Constructor_ValidEndpoints_Succeeds) {
    EXPECT_NO_THROW(SimpleReadWriteSplittingPlugin(dbc_, nullptr));
}

// ---------------------------------------------------------------------------
// SwitchConnectionIfRequired tests
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, SwitchToReaderThenWriter) {
    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_writer_hdbc_;

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
    ON_CALL(*mock_hlp_, GetConnectionRole(_)).WillByDefault(Return(READER));

    EXPECT_CALL(*mock_plugin_service_, SetCurrentHostInfo(_)).Times(testing::AtLeast(1));

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, writer_host_);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc_->wrapped_dbc, fake_reader_hdbc_);

    // Switch back to writer
    DBC* writer_dbc = new DBC();
    writer_dbc->env = &env_;
    writer_dbc->wrapped_dbc = fake_writer_hdbc_;
    writer_dbc->plugin_head = nullptr;

    ON_CALL(*mock_odbc_helper_, AllocDbc(_, _))
        .WillByDefault([writer_dbc](SQLHENV&, SQLHDBC& hdbc) -> SQLRETURN {
            hdbc = static_cast<SQLHDBC>(writer_dbc);
            return SQL_SUCCESS;
        });
    ON_CALL(*mock_hlp_, GetConnectionRole(_)).WillByDefault(Return(WRITER));

    ret = plugin->SwitchConnectionIfRequired(false, reader_host_);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc_->wrapped_dbc, fake_writer_hdbc_);
}

TEST_F(SimpleReadWriteSplittingPluginTest, AlreadyOnReader_NoSwitch) {
    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_reader_hdbc_;
    plugin->SetCurrentConnection(fake_reader_hdbc_);

    EXPECT_CALL(*mock_plugin_service_, SetCurrentHostInfo(_)).Times(0);
    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, reader_host_);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc_->wrapped_dbc, fake_reader_hdbc_);
}

TEST_F(SimpleReadWriteSplittingPluginTest, AlreadyOnWriter_NoSwitch) {
    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_writer_hdbc_;

    EXPECT_CALL(*mock_plugin_service_, SetCurrentHostInfo(_)).Times(0);
    SQLRETURN ret = plugin->SwitchConnectionIfRequired(false, writer_host_);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc_->wrapped_dbc, fake_writer_hdbc_);
}

TEST_F(SimpleReadWriteSplittingPluginTest, SwitchToWriter_InTransaction_ReturnsError) {
    auto plugin = MakePlugin();
    dbc_->wrapped_dbc = fake_reader_hdbc_;
    dbc_->transaction_status = TRANSACTION_OPEN;
    plugin->SetCurrentConnection(fake_reader_hdbc_);

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(false, reader_host_);
    EXPECT_EQ(ret, SQL_ERROR);
}

// ---------------------------------------------------------------------------
// CreateHostInfo tests
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, CreateHostInfo_ReaderRole) {
    auto plugin = MakePlugin();
    HostInfo info = plugin->CreateHostInfo(READ_ENDPOINT, READER);

    EXPECT_EQ(info.GetHost(), READ_ENDPOINT);
    EXPECT_EQ(info.GetPort(), TEST_PORT);
    EXPECT_EQ(info.GetHostRole(), READER);
}

TEST_F(SimpleReadWriteSplittingPluginTest, CreateHostInfo_WriterRole) {
    auto plugin = MakePlugin();
    HostInfo info = plugin->CreateHostInfo(WRITE_ENDPOINT, WRITER);

    EXPECT_EQ(info.GetHost(), WRITE_ENDPOINT);
    EXPECT_EQ(info.GetPort(), TEST_PORT);
    EXPECT_EQ(info.GetHostRole(), WRITER);
}

TEST_F(SimpleReadWriteSplittingPluginTest, CreateHostInfo_CustomPort) {
    dbc_->conn_attr[KEY_PORT] = "3306";
    auto plugin = MakePlugin();
    HostInfo info = plugin->CreateHostInfo(READ_ENDPOINT, READER);

    EXPECT_EQ(info.GetPort(), 3306);
}

// ---------------------------------------------------------------------------
// No-op overrides
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, RefreshAndStoreTopology_ReturnsSuccess) {
    auto plugin = MakePlugin();
    EXPECT_CALL(*mock_plugin_service_, RefreshHosts()).Times(0);

    SQLRETURN ret = plugin->RefreshAndStoreTopology();
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
}

TEST_F(SimpleReadWriteSplittingPluginTest, CloseReaderIfNecessary_NoOp) {
    auto plugin = MakePlugin();
    EXPECT_NO_THROW(plugin->CloseReaderIfNecessary());
}

// ---------------------------------------------------------------------------
// Connect tests
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, Connect_VerificationDisabled_DelegatesToNext) {
    dbc_->conn_attr[KEY_SRW_VERIFY_CONNS] = VALUE_BOOL_FALSE;
    auto plugin = MakePlugin();

    EXPECT_CALL(*mock_next_plugin_, Connect(_, _, _, _, _, _))
        .Times(1).WillOnce(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Connect_InternalSkip_DelegatesToNext) {
    dbc_->conn_attr[KEY_SRW_SKIP] = VALUE_BOOL_TRUE;
    auto plugin = MakePlugin();

    EXPECT_CALL(*mock_next_plugin_, Connect(_, _, _, _, _, _))
        .Times(1).WillOnce(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Connect_NonRdsHost_DelegatesToNext) {
    dbc_->conn_attr[KEY_SERVER] = "custom-db.example.com";
    auto plugin = MakePlugin();

    EXPECT_CALL(*mock_next_plugin_, Connect(_, _, _, _, _, _))
        .Times(1).WillOnce(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}

// ---------------------------------------------------------------------------
// ShouldUpdate tests
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, ShouldUpdateWriterConnection_NoCurrentConn) {
    auto plugin = MakePlugin();
    EXPECT_FALSE(plugin->ShouldUpdateWriterConnection(writer_host_));
}

TEST_F(SimpleReadWriteSplittingPluginTest, ShouldUpdateReaderConnection_NoCurrentConn) {
    auto plugin = MakePlugin();
    EXPECT_FALSE(plugin->ShouldUpdateReaderConnection(reader_host_));
}

// ---------------------------------------------------------------------------
// Verify initial connection type
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, Connect_VerifyInitialConnType_Writer) {
    dbc_->conn_attr[KEY_SRW_VERIFY_INITIAL_CONN_TYPE] = "WRITER";
    dbc_->conn_attr[KEY_SERVER] = "custom-db.example.com";
    auto plugin = MakePlugin();

    ON_CALL(*mock_hlp_, GetConnectionRole(_)).WillByDefault(Return(WRITER));
    EXPECT_CALL(*mock_next_plugin_, Connect(_, _, _, _, _, _))
        .Times(testing::AtLeast(1)).WillRepeatedly(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Connect_VerifyInitialConnType_Reader) {
    dbc_->conn_attr[KEY_SRW_VERIFY_INITIAL_CONN_TYPE] = "READER";
    dbc_->conn_attr[KEY_SERVER] = "custom-db.example.com";
    auto plugin = MakePlugin();

    ON_CALL(*mock_hlp_, GetConnectionRole(_)).WillByDefault(Return(READER));
    EXPECT_CALL(*mock_next_plugin_, Connect(_, _, _, _, _, _))
        .Times(testing::AtLeast(1)).WillRepeatedly(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}
