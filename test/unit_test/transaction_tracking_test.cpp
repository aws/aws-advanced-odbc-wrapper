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
#include <string>
#include <vector>

#include "../../driver/driver.h"
#include "../../driver/odbcapi_rds_helper.h"
#include "../../driver/util/plugin_service.h"
#include "../../driver/util/rds_lib_loader.h"

namespace {
    // What SQLEndTran returns for a handle; SQL_SUCCESS for one not in it.
    std::map<SQLHANDLE, SQLRETURN> g_end_tran_results;
    // The handles SQLEndTran got, in order.
    std::vector<SQLHANDLE> g_end_tran_handles;

    SQLRETURN FakeEndTran(SQLSMALLINT /*handle_type*/, SQLHANDLE handle, SQLSMALLINT /*completion_type*/) {
        g_end_tran_handles.push_back(handle);
        const auto it = g_end_tran_results.find(handle);
        return it != g_end_tran_results.end() ? it->second : SQL_SUCCESS;
    }

    SQLRETURN FakeSetConnectAttr(SQLHDBC /*connection_handle*/, SQLINTEGER /*attribute*/, SQLPOINTER /*value*/,
                                 SQLINTEGER /*string_length*/) {
        return SQL_SUCCESS;
    }

    class FakeBaseDriver : public RdsLibLoader {
    public:
        RdsPlatform::FuncHandle GetFunction(const std::string& function_name) override {
            if (function_name == RdsFuncNames::EndTran) {
                return reinterpret_cast<RdsPlatform::FuncHandle>(&FakeEndTran);
            }
            if (function_name == RdsFuncNames::SetConnectAttr) {
                return reinterpret_cast<RdsPlatform::FuncHandle>(&FakeSetConnectAttr);
            }
            return nullptr;
        }
    };
}  // namespace

// A connection in autocommit mode over a fake base driver, in the environment's list.
class TransactionTrackingTest : public testing::Test {
protected:
    std::shared_ptr<FakeBaseDriver> base_driver_ = std::make_shared<FakeBaseDriver>();
    ENV env_;
    DBC dbc_{};
    DBC second_{};
    int base_env_ = 0;
    int base_dbc_ = 0;
    int second_base_dbc_ = 0;

    void SetUp() override {
        g_end_tran_results.clear();
        g_end_tran_handles.clear();
        env_.driver_lib_loader = base_driver_;
        env_.wrapped_env = static_cast<SQLHENV>(&base_env_);
        Connect(dbc_, base_dbc_);
    }

    void Connect(DBC& dbc, int& base_dbc) {
        dbc.env = &env_;
        dbc.wrapped_dbc = static_cast<SQLHDBC>(&base_dbc);
        dbc.conn_status = CONN_CONNECTED;
        dbc.transaction_status = TRANSACTION_CLOSED;
        dbc.plugin_service = std::make_shared<PluginService>(
            base_driver_, &env_, std::map<std::string, std::string>{});
        env_.dbc_list.push_back(&dbc);
    }
};

// psqlODBC does nothing when autocommit is set on while it is already on, so a transaction opened with BEGIN stays open.
// Switching it on from off still ends the transaction.
TEST_F(TransactionTrackingTest, AutocommitSetOnAgain_KeepsABeginTransactionOpen) {
    dbc_.transaction_status = TRANSACTION_OPEN;  // as a BEGIN leaves it
    EXPECT_EQ(SQL_SUCCESS, RDS_SQLSetConnectAttr(&dbc_, SQL_ATTR_AUTOCOMMIT,
                                                 reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0));
    EXPECT_EQ(TRANSACTION_OPEN, dbc_.transaction_status);

    EXPECT_EQ(SQL_SUCCESS, RDS_SQLSetConnectAttr(&dbc_, SQL_ATTR_AUTOCOMMIT,
                                                 reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0));
    EXPECT_EQ(SQL_SUCCESS, RDS_SQLSetConnectAttr(&dbc_, SQL_ATTR_AUTOCOMMIT,
                                                 reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0));
    EXPECT_EQ(TRANSACTION_CLOSED, dbc_.transaction_status);
}

// A connection's failed commit is the result, although the later connection and the base environment succeed. Every
// connection's commit is still sent.
TEST_F(TransactionTrackingTest, EnvCommit_AConnectionFails_ReturnsItsError) {
    Connect(second_, second_base_dbc_);
    g_end_tran_results[static_cast<SQLHANDLE>(&base_dbc_)] = SQL_ERROR;
    EXPECT_EQ(SQL_ERROR, RDS_SQLEndTran(SQL_HANDLE_ENV, &env_, SQL_COMMIT));
    const std::vector<SQLHANDLE> expected = {&base_dbc_, &second_base_dbc_, &base_env_};
    EXPECT_EQ(expected, g_end_tran_handles);
}

// An allocated connection that was never connected has no transaction; the connected ones are still committed.
TEST_F(TransactionTrackingTest, EnvCommit_SkipsAConnectionThatIsNotConnected) {
    second_.env = &env_;
    env_.dbc_list.push_front(&second_);
    EXPECT_EQ(SQL_SUCCESS, RDS_SQLEndTran(SQL_HANDLE_ENV, &env_, SQL_COMMIT));
    const std::vector<SQLHANDLE> expected = {&base_dbc_, &base_env_};
    EXPECT_EQ(expected, g_end_tran_handles);
}

// With no connection ever connected there is no base environment and nothing to end.
TEST_F(TransactionTrackingTest, EnvCommit_NoConnectionConnected_ReturnsSuccess) {
    ENV env;
    second_.env = &env;
    env.dbc_list.push_back(&second_);
    EXPECT_EQ(SQL_SUCCESS, RDS_SQLEndTran(SQL_HANDLE_ENV, &env, SQL_COMMIT));
}
