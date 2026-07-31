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

#include <cstdlib>

#include <gtest/gtest.h>

#include "../../driver/odbcapi.h"
#include "../../driver/util/odbc_dsn_helper.h"
#include "../../driver/util/rds_lib_loader.h"

namespace {
const char* NONEXISTENT_MODULE = "/nonexistent/path/to/no-such-driver-module.so";

#ifndef _WIN32
const bool ODBCSYSINI_SET = [] {
    setenv("ODBCSYSINI", AWS_PROFILE_TEST_RESOURCES_DIR, 1);
    return true;
}();
#endif
} // namespace

TEST(RdsLibLoaderTest, NonexistentModuleIsNotLoaded) {
    RdsLibLoader loader(NONEXISTENT_MODULE);
    EXPECT_FALSE(loader.IsLoaded());
    EXPECT_FALSE(loader.GetLoadError().empty());
    EXPECT_EQ(NONEXISTENT_MODULE, loader.GetDriverPath());
}

TEST(RdsLibLoaderTest, GetFunctionOnUnloadedModuleReturnsNull) {
    RdsLibLoader loader(NONEXISTENT_MODULE);
    EXPECT_EQ(nullptr, loader.GetFunction("SQLDriverConnect"));
    EXPECT_EQ(nullptr, loader.GetFunction("SQLFreeHandle"));
}

TEST(RdsLibLoaderTest, CallFunctionOnUnloadedModuleFailsWithoutInvoking) {
    RdsLibLoader loader(NONEXISTENT_MODULE);
    SQLHENV env_handle = nullptr;
    const RdsLibResult res = loader.CallFunction<RDS_FP_SQLAllocHandle>(
        "SQLAllocHandle", static_cast<SQLSMALLINT>(SQL_HANDLE_ENV), nullptr, &env_handle);
    EXPECT_FALSE(res.fn_load_success);
    EXPECT_EQ(SQL_ERROR, res.fn_result);
    EXPECT_EQ(nullptr, env_handle);
}

TEST(RdsLibLoaderTest, DefaultConstructedLoaderIsNotLoaded) {
    RdsLibLoader loader;
    EXPECT_FALSE(loader.IsLoaded());
    EXPECT_EQ(nullptr, loader.GetFunction("SQLDriverConnect"));
}

TEST(ResolveDriverNameTest, UnregisteredNameReturnsEmpty) {
    EXPECT_TRUE(OdbcDsnHelper::ResolveDriverName("DefinitelyNotARegisteredOdbcDriver123").empty());
}

#ifndef _WIN32
// On Unix the odbcinst.ini location can be controlled by environment variable
// Windows Driver information lives in registry, no environment variable to control via test fixture
TEST(ResolveDriverNameTest, RegisteredNameResolvesToDriverPath) {
    EXPECT_EQ("/tmp/unit-test-fake-driver.so", OdbcDsnHelper::ResolveDriverName("UnitTestFakeDriver"));
}
#endif
