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

#include "../../driver/driver.h"
#include "../../driver/plugin/base_plugin.h"

#include <cstring>
#include <sqlext.h>

namespace {

// Plugin that records delegation instead of calling a real driver
class RecordingPlugin : public BasePlugin {
public:
    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override {
        connect_called = true;
        return SQL_SUCCESS;
    }

    SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText,
        SQLINTEGER     TextLength) override {
        execute_called = true;
        return SQL_SUCCESS;
    }

    void ReleaseResources() override {
        release_called = true;
        BasePlugin::ReleaseResources();
    }

    bool connect_called = false;
    bool execute_called = false;
    bool release_called = false;
};

// Mirrors how real plugins (failover, blue/green, etc.) delegate through the
// protected ConnectNext/ExecuteNext helpers.
class DelegatingPlugin : public BasePlugin {
public:
    DelegatingPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin) : BasePlugin(dbc, next_plugin) {}

    void SetNextPlugin(std::shared_ptr<BasePlugin> next) {
        next_plugin_ = std::move(next);
    }

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override {
        return ConnectNext(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText,
        SQLINTEGER     TextLength) override {
        return ExecuteNext(StatementHandle, StatementText, TextLength);
    }
};

}  // namespace

class BasePluginTest : public testing::Test {
protected:
    ENV* env_ = nullptr;
    DBC* dbc_ = nullptr;
    STMT* stmt_ = nullptr;

    void SetUp() override {
        env_ = new ENV();
        dbc_ = new DBC();
        dbc_->env = env_;
        stmt_ = new STMT();
        stmt_->dbc = dbc_;
    }

    void TearDown() override {
        delete stmt_;
        delete dbc_;
        delete env_;
    }
};

TEST_F(BasePluginTest, ConnectWithNullNextPluginReturnsErrorAndSetsDiagnostic) {
    BasePlugin plugin(dbc_, nullptr);

    const SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, dbc_->err);
    EXPECT_STREQ("HY000", dbc_->err->sqlstate);
    EXPECT_EQ(SQL_ERROR, dbc_->err->ret_code);
    ASSERT_NE(nullptr, dbc_->err->error_msg);
    EXPECT_NE(nullptr, strstr(dbc_->err->error_msg, "BasePlugin"));
    EXPECT_NE(nullptr, strstr(dbc_->err->error_msg, "Connect"));
}

TEST_F(BasePluginTest, ExecuteWithNullNextPluginReturnsErrorAndSetsDiagnostic) {
    BasePlugin plugin(dbc_, nullptr);

    const SQLRETURN ret = plugin.Execute(stmt_, nullptr, 0);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, stmt_->err);
    EXPECT_STREQ("HY000", stmt_->err->sqlstate);
    EXPECT_EQ(SQL_ERROR, stmt_->err->ret_code);
    ASSERT_NE(nullptr, stmt_->err->error_msg);
    EXPECT_NE(nullptr, strstr(stmt_->err->error_msg, "BasePlugin"));
    EXPECT_NE(nullptr, strstr(stmt_->err->error_msg, "Execute"));
}

TEST_F(BasePluginTest, ConnectWithNullNextPluginReplacesExistingDiagnostic) {
    BasePlugin plugin(dbc_, nullptr);
    dbc_->err = std::make_unique<ErrInfo>("Stale error from a previous operation", ERR_GENERAL_ERROR);

    EXPECT_EQ(SQL_ERROR, plugin.Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));

    ASSERT_NE(nullptr, dbc_->err);
    ASSERT_NE(nullptr, dbc_->err->error_msg);
    EXPECT_EQ(nullptr, strstr(dbc_->err->error_msg, "Stale error"));
}

TEST_F(BasePluginTest, ConnectWithNullNextPluginAndNullHandleReturnsInvalidHandle) {
    BasePlugin plugin(dbc_, nullptr);

    EXPECT_EQ(SQL_INVALID_HANDLE, plugin.Connect(nullptr, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));
}

TEST_F(BasePluginTest, ExecuteWithNullNextPluginAndNullHandleReturnsInvalidHandle) {
    BasePlugin plugin(dbc_, nullptr);

    EXPECT_EQ(SQL_INVALID_HANDLE, plugin.Execute(nullptr, nullptr, 0));
}

TEST_F(BasePluginTest, ConnectDelegatesToNextPlugin) {
    const std::shared_ptr<RecordingPlugin> next = std::make_shared<RecordingPlugin>();
    BasePlugin plugin(dbc_, next);

    const SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_TRUE(next->connect_called);
    EXPECT_EQ(nullptr, dbc_->err);
}

TEST_F(BasePluginTest, ExecuteDelegatesToNextPlugin) {
    const std::shared_ptr<RecordingPlugin> next = std::make_shared<RecordingPlugin>();
    BasePlugin plugin(dbc_, next);

    const SQLRETURN ret = plugin.Execute(stmt_, nullptr, 0);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_TRUE(next->execute_called);
    EXPECT_EQ(nullptr, stmt_->err);
}

TEST_F(BasePluginTest, ConnectWithSelfReferencingNextPluginReturnsErrorInsteadOfRecursing) {
    const std::shared_ptr<DelegatingPlugin> plugin = std::make_shared<DelegatingPlugin>(dbc_, nullptr);
    plugin->SetNextPlugin(plugin);

    const SQLRETURN ret = plugin->Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, dbc_->err);
    EXPECT_STREQ("HY000", dbc_->err->sqlstate);

    plugin->SetNextPlugin(nullptr); // Break the self-reference cycle so the plugin is freed.
}

TEST_F(BasePluginTest, ExecuteWithSelfReferencingNextPluginReturnsErrorInsteadOfRecursing) {
    const std::shared_ptr<DelegatingPlugin> plugin = std::make_shared<DelegatingPlugin>(dbc_, nullptr);
    plugin->SetNextPlugin(plugin);

    const SQLRETURN ret = plugin->Execute(stmt_, nullptr, 0);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, stmt_->err);
    EXPECT_STREQ("HY000", stmt_->err->sqlstate);

    plugin->SetNextPlugin(nullptr); // Break the self-reference cycle so the plugin is freed.
}

TEST_F(BasePluginTest, SubclassConnectWithNullNextPluginReturnsErrorAndSetsDiagnostic) {
    DelegatingPlugin plugin(dbc_, nullptr);

    const SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, dbc_->err);
    EXPECT_STREQ("HY000", dbc_->err->sqlstate);
}

TEST_F(BasePluginTest, SubclassExecuteWithNullNextPluginReturnsErrorAndSetsDiagnostic) {
    DelegatingPlugin plugin(dbc_, nullptr);

    const SQLRETURN ret = plugin.Execute(stmt_, nullptr, 0);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, stmt_->err);
    EXPECT_STREQ("HY000", stmt_->err->sqlstate);
}

TEST_F(BasePluginTest, SubclassDelegatesThroughChainToTerminalPlugin) {
    const std::shared_ptr<RecordingPlugin> terminal = std::make_shared<RecordingPlugin>();
    DelegatingPlugin plugin(dbc_, terminal);

    EXPECT_EQ(SQL_SUCCESS, plugin.Connect(dbc_, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));
    EXPECT_TRUE(terminal->connect_called);
    EXPECT_EQ(SQL_SUCCESS, plugin.Execute(stmt_, nullptr, 0));
    EXPECT_TRUE(terminal->execute_called);
}

TEST_F(BasePluginTest, ReleaseResourcesWithNullNextPluginDoesNotCrash) {
    BasePlugin plugin(dbc_, nullptr);

    plugin.ReleaseResources();
}

TEST_F(BasePluginTest, ReleaseResourcesDelegatesToNextPlugin) {
    const std::shared_ptr<RecordingPlugin> next = std::make_shared<RecordingPlugin>();
    BasePlugin plugin(dbc_, next);

    plugin.ReleaseResources();

    EXPECT_TRUE(next->release_called);
}
