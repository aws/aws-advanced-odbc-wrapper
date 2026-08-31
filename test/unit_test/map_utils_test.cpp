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
    std::map<std::string, std::string> string_string_map = {
        {"someKey0", "someValue0"}, {"someKey1", "someValue1"}, {"someKey2", "someValue2"}
    };
    std::map<std::string, std::string> string_milliseconds_map = {
        {"someKey0", "1"}, {"someKey1", "22"}, {"someKey2", "333"}
    };
    std::map<std::string, std::string> string_boolean_map = {
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
    const std::string default_value_string = "someDefaultValue";
    const std::string expected_string_value = "someValue1";
    EXPECT_STREQ(expected_string_value.c_str(), (MapUtils::GetStringValue(string_string_map, key, default_value_string)).c_str());
}

TEST_F(MapUtilsTests, GivenKeyDoesNotExistWhenGetStringValueThenReturnsDefault) {
    const std::string key = "wrongKey";
    const std::string expected_string_value = "someDefaultValue";
    EXPECT_STREQ(expected_string_value.c_str(), (MapUtils::GetStringValue(string_string_map, key, expected_string_value)).c_str());
}

TEST_F(MapUtilsTests, GivenKeyExistsWhenGetMillisecondsValueThenReturnsValue) {
    const std::string key = "someKey2";
    const std::chrono::milliseconds default_value = std::chrono::milliseconds(4444);
    const std::chrono::milliseconds expected_value = std::chrono::milliseconds(333);
    EXPECT_EQ(expected_value, MapUtils::GetMillisecondsValue(string_milliseconds_map, key, default_value));
}

TEST_F(MapUtilsTests, GivenKeyDoesNotExistsWhenGetMillisecondsValueThenReturnsValue) {
    const std::string key = "wrongKey";
    const std::chrono::milliseconds default_value = std::chrono::milliseconds(4444);
    const std::chrono::milliseconds expected_value = std::chrono::milliseconds(4444);
    EXPECT_EQ(expected_value, MapUtils::GetMillisecondsValue(string_milliseconds_map, key, default_value));
}

TEST_F(MapUtilsTests, GivenInvalidValueWhenGetMillisecondsValueThenReturnsDefault) {
    const std::map<std::string, std::string> invalid_map = {
        {"notANumber", "abc"}, {"trailingJunk", "123abc"}, {"empty", ""}, {"decimal", "1.5"}
    };
    const std::chrono::milliseconds default_value = std::chrono::milliseconds(4444);
    EXPECT_EQ(default_value, MapUtils::GetMillisecondsValue(invalid_map, "notANumber", default_value));
    EXPECT_EQ(default_value, MapUtils::GetMillisecondsValue(invalid_map, "trailingJunk", default_value));
    EXPECT_EQ(default_value, MapUtils::GetMillisecondsValue(invalid_map, "empty", default_value));
    EXPECT_EQ(default_value, MapUtils::GetMillisecondsValue(invalid_map, "decimal", default_value));
}

TEST_F(MapUtilsTests, GivenKeyExistsWhenGetSecondsValueThenReturnsValue) {
    const std::map<std::string, std::string> seconds_map = {{"someKey0", "900"}};
    const std::chrono::seconds default_value = std::chrono::seconds(10);
    EXPECT_EQ(std::chrono::seconds(900), MapUtils::GetSecondsValue(seconds_map, "someKey0", default_value));
    EXPECT_EQ(default_value, MapUtils::GetSecondsValue(seconds_map, "wrongKey", default_value));
}

TEST_F(MapUtilsTests, GivenInvalidValueWhenGetSecondsValueThenReturnsDefault) {
    const std::map<std::string, std::string> invalid_map = {{"someKey0", "15min"}};
    const std::chrono::seconds default_value = std::chrono::seconds(900);
    EXPECT_EQ(default_value, MapUtils::GetSecondsValue(invalid_map, "someKey0", default_value));
}

TEST_F(MapUtilsTests, GivenKeyExistsWhenGetIntValueThenReturnsValue) {
    const std::map<std::string, std::string> int_map = {{"someKey0", "5432"}, {"negative", "-1"}};
    EXPECT_EQ(5432, MapUtils::GetIntValue(int_map, "someKey0", 0));
    EXPECT_EQ(-1, MapUtils::GetIntValue(int_map, "negative", 0));
    EXPECT_EQ(7777, MapUtils::GetIntValue(int_map, "wrongKey", 7777));
}

TEST_F(MapUtilsTests, GivenInvalidValueWhenGetIntValueThenReturnsDefault) {
    const std::map<std::string, std::string> invalid_map = {
        {"notANumber", "port"}, {"trailingJunk", "5432x"}, {"overflow", "99999999999999999999"}
    };
    EXPECT_EQ(1111, MapUtils::GetIntValue(invalid_map, "notANumber", 1111));
    EXPECT_EQ(2222, MapUtils::GetIntValue(invalid_map, "trailingJunk", 2222));
    EXPECT_EQ(3333, MapUtils::GetIntValue(invalid_map, "overflow", 3333));
}

TEST_F(MapUtilsTests, GivenKeyExistsWhenGetBooleanValueThenReturnsValue) {
    const std::string key = "someKey1";
    const bool default_value = false;
    const bool expected_value = true;
    EXPECT_EQ(expected_value, MapUtils::GetBooleanValue(string_boolean_map, key, default_value));
}

TEST_F(MapUtilsTests, GivenKeyDoesNotExistsWhenGetBooleanValueThenReturnsValue) {
    const std::string key = "wrongKey";
    const bool default_value = false;
    const bool expected_value = false;
    EXPECT_EQ(expected_value, MapUtils::GetBooleanValue(string_boolean_map, key, default_value));
}
