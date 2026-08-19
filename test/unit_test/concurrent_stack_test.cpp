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

#include "../../driver/util/concurrent_stack.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <string>

namespace {
    const std::string ONE = "one";
    const std::string TWO = "two";
    const std::string THREE = "three";
    const std::string FOUR = "four";
    const std::string FIVE = "five";
}

class ConcurrentStackTest : public testing::Test {

protected:
    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    // Runs per test case
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConcurrentStackTest, Test_Push_And_Pop) {
    ConcurrentStack<std::string> stack;

    EXPECT_EQ(0, stack.Size());
    EXPECT_TRUE(stack.Empty());

    stack.PushBack(ONE);
    stack.PushBack(TWO);
    stack.PushBack(THREE);
    stack.PushBack(FOUR);
    stack.PushBack(FIVE);
    EXPECT_EQ(5, stack.Size());
    EXPECT_FALSE(stack.Empty());
    EXPECT_EQ(FIVE, stack.Back());

    stack.PopBack();
    EXPECT_EQ(4, stack.Size());
    EXPECT_EQ(FOUR, stack.Back());

    stack.PopBack();
    EXPECT_EQ(3, stack.Size());
    EXPECT_EQ(THREE, stack.Back());

    stack.Clear();

    EXPECT_EQ(0, stack.Size());
    EXPECT_TRUE(stack.Empty());
}

TEST_F(ConcurrentStackTest, Test_RemoveIf) {
    ConcurrentStack<std::string> stack;
    stack.PushBack(ONE);
    stack.PushBack(TWO);
    stack.PushBack(THREE);
    stack.PushBack(FOUR);
    stack.PushBack(FIVE);

    EXPECT_EQ(5, stack.Size());

    std::function<bool(std::string)> is_three = [](std::string value) { return value == THREE; };
    stack.RemoveIf(is_three);

    EXPECT_EQ(4, stack.Size());

    EXPECT_EQ(FIVE, stack.Back());

    stack.PopBack();
    EXPECT_EQ(FOUR, stack.Back());

    stack.PopBack();
    EXPECT_EQ(TWO, stack.Back());

    stack.PopBack();
    EXPECT_EQ(ONE, stack.Back());
}

TEST_F(ConcurrentStackTest, Test_ForEach) {
    ConcurrentStack<std::string> stack;
    stack.PushBack(ONE);
    stack.PushBack(TWO);
    stack.PushBack(THREE);
    stack.PushBack(FOUR);
    stack.PushBack(FIVE);

    std::string expected_string = ONE + TWO + THREE + FOUR + FIVE;

    std::string actual_string = "";

    std::function<void(std::string)> append_to_actual_string = [&actual_string](std::string value) {actual_string = actual_string + value; };
    stack.ForEach(append_to_actual_string);

    EXPECT_EQ(expected_string, actual_string);
}
