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

#include <map>
#include <string>

#include "../../driver/util/map_utils.h"

namespace {
    std::map<std::string, std::string> stringStringMap = {
        {"someKey0", "someValue0"}, {"someKey1", "someValue1"}, {"someKey2", "someValue2"}
    };
}

class MapUtilsTests : public testing::Test {
protected:
    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    void SetUp() override {}
    void TearDown() override {}
};


TEST_F(MapUtilsTests, GivenKeyExistsThenReturnsValue) {
    const std::string key = "someKey1";
    const std::string defaultValueString = "someDefaultValue";
    const std::string expectedStringValue = "someValue1";
    EXPECT_STREQ(expectedStringValue.c_str(), (MapUtils::GetValue(stringStringMap, key, defaultValueString)).c_str());
}

TEST_F(MapUtilsTests, GivenKeyDoesNotExistThenReturnsDefault) {
    const std::string key = "wrongKey";
    const std::string expectedStringValue = "someDefaultValue";
    EXPECT_STREQ(expectedStringValue.c_str(), (MapUtils::GetValue(stringStringMap, key, expectedStringValue)).c_str());
}

