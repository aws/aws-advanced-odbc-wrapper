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

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "../../driver/util/odbc_helper.h"
#include "../../driver/util/rds_lib_loader.h"

// The base driver's character width is a property of the driver, not of one helper or one ENV. The
// topology monitor runs its queries through a different OdbcHelper than the connection that detected
// the width, and a connection from another ENV can hand the monitor its own helper.
//
// The width is kept per driver path for the life of the process, so each test uses a path of its own
// to keep state from leaking between tests. The paths do not exist; the loaders just fail to load.
namespace {
std::shared_ptr<RdsLibLoader> LoaderFor(const std::string& test_name) {
    return std::make_shared<RdsLibLoader>("/nonexistent/odbc_helper_test/" + test_name + ".so");
}
} // namespace

TEST(OdbcHelperTest, DefaultsTo2ByteBaseDriver) {
    const OdbcHelper helper(LoaderFor("DefaultsTo2ByteBaseDriver"), nullptr);
    EXPECT_FALSE(helper.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_SharedBetweenHelpersOnSameLoader) {
    const auto loader = LoaderFor("SharedBetweenHelpersOnSameLoader");
    OdbcHelper main_helper(loader, nullptr);
    const OdbcHelper monitor_helper(loader, nullptr);

    main_helper.SetUse4BytesBaseDriver(true);

    EXPECT_TRUE(main_helper.GetUse4BytesBaseDriver());
    EXPECT_TRUE(monitor_helper.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_VisibleToHelperCreatedAfterItWasSet) {
    const auto loader = LoaderFor("VisibleToHelperCreatedAfterItWasSet");
    OdbcHelper main_helper(loader, nullptr);
    main_helper.SetUse4BytesBaseDriver(true);

    const OdbcHelper late_helper(loader, nullptr);

    EXPECT_TRUE(late_helper.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_CanBeClearedForAllHelpers) {
    const auto loader = LoaderFor("CanBeClearedForAllHelpers");
    OdbcHelper main_helper(loader, nullptr);
    const OdbcHelper monitor_helper(loader, nullptr);

    main_helper.SetUse4BytesBaseDriver(true);
    main_helper.SetUse4BytesBaseDriver(false);

    EXPECT_FALSE(main_helper.GetUse4BytesBaseDriver());
    EXPECT_FALSE(monitor_helper.GetUse4BytesBaseDriver());
}

// A second ENV gets its own loader for the same driver. A monitor it joins must still see the width
// the first ENV's connection detected, including a loader created after the width was set.
TEST(OdbcHelperTest, Flag_SharedAcrossLoadersForSameDriverPath) {
    const std::string test_name = "SharedAcrossLoadersForSameDriverPath";
    const auto first_env_loader = LoaderFor(test_name);
    OdbcHelper first_env_helper(first_env_loader, nullptr);
    const auto second_env_loader = LoaderFor(test_name);
    const OdbcHelper second_env_helper(second_env_loader, nullptr);

    first_env_helper.SetUse4BytesBaseDriver(true);

    EXPECT_TRUE(second_env_helper.GetUse4BytesBaseDriver());
    EXPECT_TRUE(second_env_loader->GetUse4BytesBaseDriver());

    const OdbcHelper third_env_helper(LoaderFor(test_name), nullptr);
    EXPECT_TRUE(third_env_helper.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_NotSharedAcrossDifferentDriverPaths) {
    OdbcHelper helper_a(LoaderFor("NotSharedAcrossDifferentDriverPaths_A"), nullptr);
    const OdbcHelper helper_b(LoaderFor("NotSharedAcrossDifferentDriverPaths_B"), nullptr);

    helper_a.SetUse4BytesBaseDriver(true);

    EXPECT_TRUE(helper_a.GetUse4BytesBaseDriver());
    EXPECT_FALSE(helper_b.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_NotSharedBetweenLoadersWithoutDriverPath) {
    OdbcHelper helper_a(std::make_shared<RdsLibLoader>(), nullptr);
    const OdbcHelper helper_b(std::make_shared<RdsLibLoader>(), nullptr);

    helper_a.SetUse4BytesBaseDriver(true);

    EXPECT_TRUE(helper_a.GetUse4BytesBaseDriver());
    EXPECT_FALSE(helper_b.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_WorksWithoutLoader) {
    OdbcHelper helper(nullptr, nullptr);

    helper.SetUse4BytesBaseDriver(true);

    EXPECT_TRUE(helper.GetUse4BytesBaseDriver());
}
