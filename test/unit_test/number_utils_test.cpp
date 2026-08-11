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

#include <cstdint>
#include <limits>
#include <string>

#include "../../driver/util/number_utils.h"

class NumberUtilsTest : public testing::Test {};

TEST_F(NumberUtilsTest, ParseIntValidValues) {
    EXPECT_EQ(0, NumberUtils::ParseInt("0").value());
    EXPECT_EQ(1234, NumberUtils::ParseInt("1234").value());
    EXPECT_EQ(-1234, NumberUtils::ParseInt("-1234").value());
    EXPECT_EQ(std::numeric_limits<int>::max(),
        NumberUtils::ParseInt(std::to_string(std::numeric_limits<int>::max())).value());
    EXPECT_EQ(std::numeric_limits<int>::min(),
        NumberUtils::ParseInt(std::to_string(std::numeric_limits<int>::min())).value());
}

TEST_F(NumberUtilsTest, ParseIntToleratesSurroundingWhitespaceAndPlusSign) {
    EXPECT_EQ(123, NumberUtils::ParseInt(" 123").value());
    EXPECT_EQ(123, NumberUtils::ParseInt("123 ").value());
    EXPECT_EQ(-123, NumberUtils::ParseInt("\t-123 ").value());
    EXPECT_EQ(123, NumberUtils::ParseInt("+123").value());
}

TEST_F(NumberUtilsTest, ParseIntInvalidValues) {
    EXPECT_FALSE(NumberUtils::ParseInt("").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("   ").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("abc").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("123abc").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("abc123").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("1.5").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("1e2").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("12 3").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("0x1A").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("+").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("+-1").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("--1").has_value());
}

TEST_F(NumberUtilsTest, ParseIntOutOfRange) {
    EXPECT_FALSE(NumberUtils::ParseInt("99999999999999999999").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("-99999999999999999999").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt("2147483648").has_value());   // INT_MAX + 1
    EXPECT_FALSE(NumberUtils::ParseInt("-2147483649").has_value());  // INT_MIN - 1
}

TEST_F(NumberUtilsTest, ParseInt64ValidValues) {
    EXPECT_EQ(0, NumberUtils::ParseInt64("0").value());
    EXPECT_EQ(2147483648LL, NumberUtils::ParseInt64("2147483648").value());  // > INT_MAX
    EXPECT_EQ(std::numeric_limits<int64_t>::max(),
        NumberUtils::ParseInt64(std::to_string(std::numeric_limits<int64_t>::max())).value());
    EXPECT_EQ(std::numeric_limits<int64_t>::min(),
        NumberUtils::ParseInt64(std::to_string(std::numeric_limits<int64_t>::min())).value());
}

TEST_F(NumberUtilsTest, ParseInt64InvalidValues) {
    EXPECT_FALSE(NumberUtils::ParseInt64("").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt64("abc").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt64("1.5").has_value());
    EXPECT_FALSE(NumberUtils::ParseInt64("99999999999999999999").has_value());  // > INT64_MAX
}
