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

#ifndef BASE_CONNECTION_TEST_H_
#define BASE_CONNECTION_TEST_H_

#include <gtest/gtest.h>

#ifdef WIN32
    #include <windows.h>
#endif

#include <thread>
#include <chrono>
#include <sql.h>
#include <sqlext.h>

#include <thread>
#include <chrono>

#include "../common/odbc_helper.h"
#include "../common/test_utils.h"

class BaseConnectionTest : public testing::Test {
protected:
    std::string test_dsn = TEST_UTILS::GetEnvVar("TEST_DSN");
    std::string test_db = TEST_UTILS::GetEnvVar("TEST_DATABASE");
    std::string test_uid = TEST_UTILS::GetEnvVar("TEST_USERNAME");
    std::string test_pwd = TEST_UTILS::GetEnvVar("TEST_PASSWORD");
    int test_port = TEST_UTILS::StrToInt(TEST_UTILS::GetEnvVar("TEST_PORT", "5432"));
    std::string test_region = TEST_UTILS::GetEnvVar("TEST_REGION", "us-west-1");
    std::string test_server = TEST_UTILS::GetEnvVar("TEST_SERVER");
    std::string test_dialect = TEST_UTILS::GetEnvVar("TEST_DIALECT");

    std::string conn_str = "";

    SQLHENV env = nullptr;
    SQLHDBC dbc = nullptr;

    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    void SetUp() override {
        // Workaround for log creation
        std::this_thread::sleep_for(std::chrono::seconds(1));

        ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env));
        ASSERT_EQ(SQL_SUCCESS, SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
        ASSERT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc));
    }
    void TearDown() override {
        ODBC_HELPER::CleanUpHandles(env, dbc, nullptr);
    }
private:
};

#endif  // BASE_CONNECTION_TEST_H_
