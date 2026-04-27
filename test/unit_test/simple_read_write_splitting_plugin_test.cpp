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

    HostInfo writer_host{WRITE_ENDPOINT, TEST_PORT, UP, WRITER};
    HostInfo reader_host{READ_ENDPOINT, TEST_PORT, UP, READER};

    std::shared_ptr<NiceMock<MOCK_PLUGIN_SERVICE>> mock_plugin_service;
    std::shared_ptr<NiceMock<MOCK_ODBC_HELPER>> mock_odbc_helper;
    std::shared_ptr<NiceMock<MOCK_HOST_LIST_PROVIDER>> mock_hlp;
    std::shared_ptr<NiceMock<MOCK_DIALECT>> mock_dialect;
    std::shared_ptr<RW_MockRdsLibLoader> mock_lib_loader;

    ENV env;
    DBC* dbc = nullptr;
    // mock_next_plugin is owned by the plugin via next_plugin chain.
    // Tests that don't create a plugin must clean it up manually.
    MOCK_BASE_PLUGIN* mock_next_plugin = nullptr;
    bool plugin_owns_mock_ = false;

    SQLHDBC fake_writer_hdbc = reinterpret_cast<SQLHDBC>(0x3000);
    SQLHDBC fake_reader_hdbc = reinterpret_cast<SQLHDBC>(0x4000);

    void SetUp() override {
        mock_lib_loader = std::make_shared<RW_MockRdsLibLoader>();
        mock_plugin_service = std::make_shared<NiceMock<MOCK_PLUGIN_SERVICE>>();
        mock_odbc_helper = std::make_shared<NiceMock<MOCK_ODBC_HELPER>>();
        mock_hlp = std::make_shared<NiceMock<MOCK_HOST_LIST_PROVIDER>>();
        mock_dialect = std::make_shared<NiceMock<MOCK_DIALECT>>();

        env.driver_lib_loader = mock_lib_loader;

        dbc = new DBC();
        dbc->env = &env;
        dbc->wrapped_dbc = nullptr;
        dbc->transaction_status = TRANSACTION_CLOSED;
        dbc->plugin_service = mock_plugin_service;

        mock_next_plugin = new MOCK_BASE_PLUGIN();
        dbc->plugin_head = mock_next_plugin;

        dbc->conn_attr[KEY_SRW_WRITE_ENDPOINT] = WRITE_ENDPOINT;
        dbc->conn_attr[KEY_SRW_READ_ENDPOINT] = READ_ENDPOINT;

        ON_CALL(*mock_plugin_service, GetOdbcHelper()).WillByDefault(Return(mock_odbc_helper));
        ON_CALL(*mock_plugin_service, GetHostListProvider()).WillByDefault(Return(mock_hlp));
        ON_CALL(*mock_plugin_service, GetDialect()).WillByDefault(Return(mock_dialect));
        ON_CALL(*mock_plugin_service, GetHosts()).WillByDefault(Return(std::vector<HostInfo>{}));
        ON_CALL(*mock_plugin_service, GetCurrentHostInfo()).WillByDefault(Return(writer_host));
        ON_CALL(*mock_plugin_service, GetOriginalConnAttr())
            .WillByDefault(Return(dbc->conn_attr));

        ON_CALL(*mock_dialect, GetDefaultPort()).WillByDefault(Return(TEST_PORT));
        ON_CALL(*mock_hlp, GetConnectionRole(_)).WillByDefault(Return(UNKNOWN));
    }

    void TearDown() override {
        if (dbc) {
            dbc->plugin_head = nullptr;
            delete dbc;
        }
        // Only delete mock_next_plugin if no plugin took ownership
        if (!plugin_owns_mock_) {
            delete mock_next_plugin;
        }
    }

    // Creates a plugin that owns mock_next_plugin via the next_plugin chain.
    // BasePlugin::~BasePlugin will delete it, so TearDown must not.
    std::unique_ptr<TestableSimpleReadWriteSplittingPlugin> MakePlugin() {
        auto plugin = std::make_unique<TestableSimpleReadWriteSplittingPlugin>(dbc, mock_next_plugin);
        plugin->SetPluginHead(mock_next_plugin);
        plugin_owns_mock_ = true;
        return plugin;
    }
};

// ---------------------------------------------------------------------------
// Constructor tests (pass nullptr — no next_plugin needed)
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, Constructor_MissingWriteEndpoint_Throws) {
    dbc->conn_attr.erase(KEY_SRW_WRITE_ENDPOINT);
    EXPECT_THROW(SimpleReadWriteSplittingPlugin(dbc, nullptr), std::runtime_error);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Constructor_MissingReadEndpoint_Throws) {
    dbc->conn_attr.erase(KEY_SRW_READ_ENDPOINT);
    EXPECT_THROW(SimpleReadWriteSplittingPlugin(dbc, nullptr), std::runtime_error);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Constructor_MissingBothEndpoints_Throws) {
    dbc->conn_attr.erase(KEY_SRW_WRITE_ENDPOINT);
    dbc->conn_attr.erase(KEY_SRW_READ_ENDPOINT);
    EXPECT_THROW(SimpleReadWriteSplittingPlugin(dbc, nullptr), std::runtime_error);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Constructor_ValidEndpoints_Succeeds) {
    EXPECT_NO_THROW(SimpleReadWriteSplittingPlugin(dbc, nullptr));
}

// ---------------------------------------------------------------------------
// SwitchConnectionIfRequired tests
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, SwitchToReaderThenWriter) {
    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_writer_hdbc;

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
    ON_CALL(*mock_hlp, GetConnectionRole(_)).WillByDefault(Return(READER));

    EXPECT_CALL(*mock_plugin_service, SetCurrentHostInfo(_)).Times(testing::AtLeast(1));

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, writer_host);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc->wrapped_dbc, fake_reader_hdbc);

    // Switch back to writer
    DBC* writer_dbc = new DBC();
    writer_dbc->env = &env;
    writer_dbc->wrapped_dbc = fake_writer_hdbc;
    writer_dbc->plugin_head = nullptr;

    ON_CALL(*mock_odbc_helper, AllocDbc(_, _))
        .WillByDefault([writer_dbc](SQLHENV&, SQLHDBC& hdbc) -> SQLRETURN {
            hdbc = static_cast<SQLHDBC>(writer_dbc);
            return SQL_SUCCESS;
        });
    ON_CALL(*mock_hlp, GetConnectionRole(_)).WillByDefault(Return(WRITER));

    ret = plugin->SwitchConnectionIfRequired(false, reader_host);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc->wrapped_dbc, fake_writer_hdbc);
}

TEST_F(SimpleReadWriteSplittingPluginTest, AlreadyOnReader_NoSwitch) {
    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_reader_hdbc;

    EXPECT_CALL(*mock_plugin_service, SetCurrentHostInfo(_)).Times(0);
    SQLRETURN ret = plugin->SwitchConnectionIfRequired(true, reader_host);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc->wrapped_dbc, fake_reader_hdbc);
}

TEST_F(SimpleReadWriteSplittingPluginTest, AlreadyOnWriter_NoSwitch) {
    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_writer_hdbc;

    EXPECT_CALL(*mock_plugin_service, SetCurrentHostInfo(_)).Times(0);
    SQLRETURN ret = plugin->SwitchConnectionIfRequired(false, writer_host);
    EXPECT_TRUE(SQL_SUCCEEDED(ret));
    EXPECT_EQ(dbc->wrapped_dbc, fake_writer_hdbc);
}

TEST_F(SimpleReadWriteSplittingPluginTest, SwitchToWriter_InTransaction_ReturnsError) {
    auto plugin = MakePlugin();
    dbc->wrapped_dbc = fake_reader_hdbc;
    dbc->transaction_status = TRANSACTION_OPEN;
    plugin->SetCurrentConnection(fake_reader_hdbc);

    SQLRETURN ret = plugin->SwitchConnectionIfRequired(false, reader_host);
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
    dbc->conn_attr[KEY_PORT] = "3306";
    auto plugin = MakePlugin();
    HostInfo info = plugin->CreateHostInfo(READ_ENDPOINT, READER);

    EXPECT_EQ(info.GetPort(), 3306);
}

// ---------------------------------------------------------------------------
// No-op overrides
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, RefreshAndStoreTopology_ReturnsSuccess) {
    auto plugin = MakePlugin();
    EXPECT_CALL(*mock_plugin_service, RefreshHosts()).Times(0);

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
    dbc->conn_attr[KEY_SRW_VERIFY_CONNS] = VALUE_BOOL_FALSE;
    auto plugin = MakePlugin();

    EXPECT_CALL(*mock_next_plugin, Connect(_, _, _, _, _, _))
        .Times(1).WillOnce(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Connect_InternalSkip_DelegatesToNext) {
    dbc->conn_attr[KEY_SRW_SKIP] = VALUE_BOOL_TRUE;
    auto plugin = MakePlugin();

    EXPECT_CALL(*mock_next_plugin, Connect(_, _, _, _, _, _))
        .Times(1).WillOnce(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Connect_NonRdsHost_DelegatesToNext) {
    dbc->conn_attr[KEY_SERVER] = "custom-db.example.com";
    auto plugin = MakePlugin();

    EXPECT_CALL(*mock_next_plugin, Connect(_, _, _, _, _, _))
        .Times(1).WillOnce(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}

// ---------------------------------------------------------------------------
// ShouldUpdate tests
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, ShouldUpdateWriterConnection_NoCurrentConn) {
    auto plugin = MakePlugin();
    EXPECT_FALSE(plugin->ShouldUpdateWriterConnection(writer_host));
}

TEST_F(SimpleReadWriteSplittingPluginTest, ShouldUpdateReaderConnection_NoCurrentConn) {
    auto plugin = MakePlugin();
    EXPECT_FALSE(plugin->ShouldUpdateReaderConnection(reader_host));
}

// ---------------------------------------------------------------------------
// Verify initial connection type
// ---------------------------------------------------------------------------

TEST_F(SimpleReadWriteSplittingPluginTest, Connect_VerifyInitialConnType_Writer) {
    dbc->conn_attr[KEY_SRW_VERIFY_INITIAL_CONN_TYPE] = "WRITER";
    dbc->conn_attr[KEY_SERVER] = "custom-db.example.com";
    auto plugin = MakePlugin();

    ON_CALL(*mock_hlp, GetConnectionRole(_)).WillByDefault(Return(WRITER));
    EXPECT_CALL(*mock_next_plugin, Connect(_, _, _, _, _, _))
        .Times(testing::AtLeast(1)).WillRepeatedly(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SimpleReadWriteSplittingPluginTest, Connect_VerifyInitialConnType_Reader) {
    dbc->conn_attr[KEY_SRW_VERIFY_INITIAL_CONN_TYPE] = "READER";
    dbc->conn_attr[KEY_SERVER] = "custom-db.example.com";
    auto plugin = MakePlugin();

    ON_CALL(*mock_hlp, GetConnectionRole(_)).WillByDefault(Return(READER));
    EXPECT_CALL(*mock_next_plugin, Connect(_, _, _, _, _, _))
        .Times(testing::AtLeast(1)).WillRepeatedly(Return(SQL_SUCCESS));

    SQLRETURN ret = plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(ret, SQL_SUCCESS);
}
