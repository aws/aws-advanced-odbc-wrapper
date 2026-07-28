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
        next_plugin = std::move(next);
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
    ENV* env = nullptr;
    DBC* dbc = nullptr;
    STMT* stmt = nullptr;

    void SetUp() override {
        env = new ENV();
        dbc = new DBC();
        dbc->env = env;
        stmt = new STMT();
        stmt->dbc = dbc;
    }

    void TearDown() override {
        delete stmt;
        delete dbc;
        delete env;
    }
};

TEST_F(BasePluginTest, ConnectWithNullNextPluginReturnsErrorAndSetsDiagnostic) {
    BasePlugin plugin(dbc, nullptr);

    const SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, dbc->err);
    EXPECT_STREQ("HY000", dbc->err->sqlstate);
    EXPECT_EQ(SQL_ERROR, dbc->err->ret_code);
    ASSERT_NE(nullptr, dbc->err->error_msg);
    EXPECT_NE(nullptr, strstr(dbc->err->error_msg, "BasePlugin"));
    EXPECT_NE(nullptr, strstr(dbc->err->error_msg, "Connect"));
}

TEST_F(BasePluginTest, ExecuteWithNullNextPluginReturnsErrorAndSetsDiagnostic) {
    BasePlugin plugin(dbc, nullptr);

    const SQLRETURN ret = plugin.Execute(stmt, nullptr, 0);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, stmt->err);
    EXPECT_STREQ("HY000", stmt->err->sqlstate);
    EXPECT_EQ(SQL_ERROR, stmt->err->ret_code);
    ASSERT_NE(nullptr, stmt->err->error_msg);
    EXPECT_NE(nullptr, strstr(stmt->err->error_msg, "BasePlugin"));
    EXPECT_NE(nullptr, strstr(stmt->err->error_msg, "Execute"));
}

TEST_F(BasePluginTest, ConnectWithNullNextPluginReplacesExistingDiagnostic) {
    BasePlugin plugin(dbc, nullptr);
    dbc->err = std::make_unique<ERR_INFO>("Stale error from a previous operation", ERR_GENERAL_ERROR);

    EXPECT_EQ(SQL_ERROR, plugin.Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));

    ASSERT_NE(nullptr, dbc->err);
    ASSERT_NE(nullptr, dbc->err->error_msg);
    EXPECT_EQ(nullptr, strstr(dbc->err->error_msg, "Stale error"));
}

TEST_F(BasePluginTest, ConnectWithNullNextPluginAndNullHandleReturnsInvalidHandle) {
    BasePlugin plugin(dbc, nullptr);

    EXPECT_EQ(SQL_INVALID_HANDLE, plugin.Connect(nullptr, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));
}

TEST_F(BasePluginTest, ExecuteWithNullNextPluginAndNullHandleReturnsInvalidHandle) {
    BasePlugin plugin(dbc, nullptr);

    EXPECT_EQ(SQL_INVALID_HANDLE, plugin.Execute(nullptr, nullptr, 0));
}

TEST_F(BasePluginTest, ConnectDelegatesToNextPlugin) {
    const std::shared_ptr<RecordingPlugin> next = std::make_shared<RecordingPlugin>();
    BasePlugin plugin(dbc, next);

    const SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_TRUE(next->connect_called);
    EXPECT_EQ(nullptr, dbc->err);
}

TEST_F(BasePluginTest, ExecuteDelegatesToNextPlugin) {
    const std::shared_ptr<RecordingPlugin> next = std::make_shared<RecordingPlugin>();
    BasePlugin plugin(dbc, next);

    const SQLRETURN ret = plugin.Execute(stmt, nullptr, 0);

    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_TRUE(next->execute_called);
    EXPECT_EQ(nullptr, stmt->err);
}

TEST_F(BasePluginTest, ConnectWithSelfReferencingNextPluginReturnsErrorInsteadOfRecursing) {
    const std::shared_ptr<DelegatingPlugin> plugin = std::make_shared<DelegatingPlugin>(dbc, nullptr);
    plugin->SetNextPlugin(plugin);

    const SQLRETURN ret = plugin->Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, dbc->err);
    EXPECT_STREQ("HY000", dbc->err->sqlstate);

    plugin->SetNextPlugin(nullptr); // Break the self-reference cycle so the plugin is freed.
}

TEST_F(BasePluginTest, ExecuteWithSelfReferencingNextPluginReturnsErrorInsteadOfRecursing) {
    const std::shared_ptr<DelegatingPlugin> plugin = std::make_shared<DelegatingPlugin>(dbc, nullptr);
    plugin->SetNextPlugin(plugin);

    const SQLRETURN ret = plugin->Execute(stmt, nullptr, 0);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, stmt->err);
    EXPECT_STREQ("HY000", stmt->err->sqlstate);

    plugin->SetNextPlugin(nullptr); // Break the self-reference cycle so the plugin is freed.
}

TEST_F(BasePluginTest, SubclassConnectWithNullNextPluginReturnsErrorAndSetsDiagnostic) {
    DelegatingPlugin plugin(dbc, nullptr);

    const SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, dbc->err);
    EXPECT_STREQ("HY000", dbc->err->sqlstate);
}

TEST_F(BasePluginTest, SubclassExecuteWithNullNextPluginReturnsErrorAndSetsDiagnostic) {
    DelegatingPlugin plugin(dbc, nullptr);

    const SQLRETURN ret = plugin.Execute(stmt, nullptr, 0);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, stmt->err);
    EXPECT_STREQ("HY000", stmt->err->sqlstate);
}

TEST_F(BasePluginTest, SubclassDelegatesThroughChainToTerminalPlugin) {
    const std::shared_ptr<RecordingPlugin> terminal = std::make_shared<RecordingPlugin>();
    DelegatingPlugin plugin(dbc, terminal);

    EXPECT_EQ(SQL_SUCCESS, plugin.Connect(dbc, nullptr, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));
    EXPECT_TRUE(terminal->connect_called);
    EXPECT_EQ(SQL_SUCCESS, plugin.Execute(stmt, nullptr, 0));
    EXPECT_TRUE(terminal->execute_called);
}

TEST_F(BasePluginTest, ReleaseResourcesWithNullNextPluginDoesNotCrash) {
    BasePlugin plugin(dbc, nullptr);

    plugin.ReleaseResources();
}

TEST_F(BasePluginTest, ReleaseResourcesDelegatesToNextPlugin) {
    const std::shared_ptr<RecordingPlugin> next = std::make_shared<RecordingPlugin>();
    BasePlugin plugin(dbc, next);

    plugin.ReleaseResources();

    EXPECT_TRUE(next->release_called);
}
