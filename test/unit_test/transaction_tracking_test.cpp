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

#include "../../driver/driver.h"
#include "../../driver/odbcapi_rds_helper.h"
#include "../../driver/util/plugin_service.h"
#include "../../driver/util/rds_lib_loader.h"

namespace {
    SQLRETURN FakeSetConnectAttr(SQLHDBC /*connection_handle*/, SQLINTEGER /*attribute*/, SQLPOINTER /*value*/,
                                 SQLINTEGER /*string_length*/) {
        return SQL_SUCCESS;
    }

    class FakeBaseDriver : public RdsLibLoader {
    public:
        RdsPlatform::FuncHandle GetFunction(const std::string& function_name) override {
            if (function_name == RdsFuncNames::SetConnectAttr) {
                return reinterpret_cast<RdsPlatform::FuncHandle>(&FakeSetConnectAttr);
            }
            return nullptr;
        }
    };
}  // namespace

// A connection in autocommit mode over a fake base driver.
class TransactionTrackingTest : public testing::Test {
protected:
    std::shared_ptr<FakeBaseDriver> base_driver_ = std::make_shared<FakeBaseDriver>();
    ENV env_;
    DBC dbc_{};
    int base_dbc_ = 0;

    void SetUp() override {
        env_.driver_lib_loader = base_driver_;
        Connect(dbc_, base_dbc_);
    }

    void Connect(DBC& dbc, int& base_dbc) {
        dbc.env = &env_;
        dbc.wrapped_dbc = static_cast<SQLHDBC>(&base_dbc);
        dbc.conn_status = CONN_CONNECTED;
        dbc.transaction_status = TRANSACTION_CLOSED;
        dbc.plugin_service = std::make_shared<PluginService>(
            base_driver_, &env_, std::map<std::string, std::string>{});
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
