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

#include <gtest/gtest.h>

#include "../../driver/util/odbc_helper.h"
#include "../../driver/util/rds_lib_loader.h"

// The base driver's character width is a property of the driver, not of one helper. The topology
// monitor runs its queries through a different OdbcHelper than the connection that detected the width.
TEST(OdbcHelperTest, DefaultsTo2ByteBaseDriver) {
    const OdbcHelper helper(std::make_shared<RdsLibLoader>(), nullptr);
    EXPECT_FALSE(helper.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_SharedBetweenHelpersOnSameLoader) {
    const auto loader = std::make_shared<RdsLibLoader>();
    OdbcHelper main_helper(loader, nullptr);
    const OdbcHelper monitor_helper(loader, nullptr);

    main_helper.SetUse4BytesBaseDriver(true);

    EXPECT_TRUE(main_helper.GetUse4BytesBaseDriver());
    EXPECT_TRUE(monitor_helper.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_VisibleToHelperCreatedAfterItWasSet) {
    const auto loader = std::make_shared<RdsLibLoader>();
    OdbcHelper main_helper(loader, nullptr);
    main_helper.SetUse4BytesBaseDriver(true);

    const OdbcHelper late_helper(loader, nullptr);

    EXPECT_TRUE(late_helper.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_CanBeClearedForAllHelpers) {
    const auto loader = std::make_shared<RdsLibLoader>();
    OdbcHelper main_helper(loader, nullptr);
    const OdbcHelper monitor_helper(loader, nullptr);

    main_helper.SetUse4BytesBaseDriver(true);
    main_helper.SetUse4BytesBaseDriver(false);

    EXPECT_FALSE(main_helper.GetUse4BytesBaseDriver());
    EXPECT_FALSE(monitor_helper.GetUse4BytesBaseDriver());
}

TEST(OdbcHelperTest, Flag_NotSharedAcrossDifferentLoaders) {
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
