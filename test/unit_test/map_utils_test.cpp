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

#include <chrono>
#include <map>
#include <string>

#include "../../driver/util/connection_string_keys.h"
#include "../../driver/util/map_utils.h"


namespace {
    std::map<std::string, std::string> stringStringMap = {
        {"someKey0", "someValue0"}, {"someKey1", "someValue1"}, {"someKey2", "someValue2"}
    };
    std::map<std::string, std::string> stringMillisecondsMap = {
        {"someKey0", "1"}, {"someKey1", "22"}, {"someKey2", "333"}
    };
    std::map<std::string, std::string> stringBooleanMap = {
        {"someKey0", "0"}, {"someKey1", "1"}, {"someKey2", "0"}
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


TEST_F(MapUtilsTests, GivenKeyExistsWhenGetStringValueThenReturnsValue) {
    const std::string key = "someKey1";
    const std::string defaultValueString = "someDefaultValue";
    const std::string expectedStringValue = "someValue1";
    EXPECT_STREQ(expectedStringValue.c_str(), (MapUtils::GetStringValue(stringStringMap, key, defaultValueString)).c_str());
}

TEST_F(MapUtilsTests, GivenKeyDoesNotExistWhenGetStringValueThenReturnsDefault) {
    const std::string key = "wrongKey";
    const std::string expectedStringValue = "someDefaultValue";
    EXPECT_STREQ(expectedStringValue.c_str(), (MapUtils::GetStringValue(stringStringMap, key, expectedStringValue)).c_str());
}

TEST_F(MapUtilsTests, GivenKeyExistsWhenGetMillisecondsValueThenReturnsValue) {
    const std::string key = "someKey2";
    const std::chrono::milliseconds defaultValue = std::chrono::milliseconds(4444);
    const std::chrono::milliseconds expectedValue = std::chrono::milliseconds(333);
    EXPECT_EQ(expectedValue, MapUtils::GetMillisecondsValue(stringMillisecondsMap, key, defaultValue));
}

TEST_F(MapUtilsTests, GivenKeyDoesNotExistsWhenGetMillisecondsValueThenReturnsValue) {
    const std::string key = "wrongKey";
    const std::chrono::milliseconds defaultValue = std::chrono::milliseconds(4444);
    const std::chrono::milliseconds expectedValue = std::chrono::milliseconds(4444);
    EXPECT_EQ(expectedValue, MapUtils::GetMillisecondsValue(stringMillisecondsMap, key, defaultValue));
}

TEST_F(MapUtilsTests, GivenKeyExistsWhenGetBooleanValueThenReturnsValue) {
    const std::string key = "someKey1";
    const bool defaultValue = false;
    const bool expectedValue = true;
    EXPECT_EQ(expectedValue, MapUtils::GetBooleanValue(stringBooleanMap, key, defaultValue));
}

TEST_F(MapUtilsTests, GivenKeyDoesNotExistsWhenGetBooleanValueThenReturnsValue) {
    const std::string key = "wrongKey";
    const bool defaultValue = false;
    const bool expectedValue = false;
    EXPECT_EQ(expectedValue, MapUtils::GetBooleanValue(stringBooleanMap, key, defaultValue));
}
