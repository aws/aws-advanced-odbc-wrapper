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
    const std::string one = "one";
    const std::string two = "two";
    const std::string three = "three";
    const std::string four = "four";
    const std::string five = "five";
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

    stack.PushBack(one);
    stack.PushBack(two);
    stack.PushBack(three);
    stack.PushBack(four);
    stack.PushBack(five);
    EXPECT_EQ(5, stack.Size());
    EXPECT_FALSE(stack.Empty());
    EXPECT_EQ(five, stack.Back());

    stack.PopBack();
    EXPECT_EQ(4, stack.Size());
    EXPECT_EQ(four, stack.Back());

    stack.PopBack();
    EXPECT_EQ(3, stack.Size());
    EXPECT_EQ(three, stack.Back());

    stack.Clear();

    EXPECT_EQ(0, stack.Size());
    EXPECT_TRUE(stack.Empty());
}

TEST_F(ConcurrentStackTest, Test_RemoveIf) {
    ConcurrentStack<std::string> stack;
    stack.PushBack(one);
    stack.PushBack(two);
    stack.PushBack(three);
    stack.PushBack(four);
    stack.PushBack(five);

    EXPECT_EQ(5, stack.Size());

    std::function<bool(std::string)> isThree = [](std::string value) { return value == three; };
    stack.RemoveIf(isThree);

    EXPECT_EQ(4, stack.Size());

    EXPECT_EQ(five, stack.Back());

    stack.PopBack();
    EXPECT_EQ(four, stack.Back());

    stack.PopBack();
    EXPECT_EQ(two, stack.Back());

    stack.PopBack();
    EXPECT_EQ(one, stack.Back());
}

TEST_F(ConcurrentStackTest, Test_ForEach) {
    ConcurrentStack<std::string> stack;
    stack.PushBack(one);
    stack.PushBack(two);
    stack.PushBack(three);
    stack.PushBack(four);
    stack.PushBack(five);

    std::string expectedString = one + two + three + four + five;

    std::string actualString = "";

    std::function<void(std::string)> appendToActualString = [&actualString](std::string value) {actualString = actualString + value; };
    stack.ForEach(appendToActualString);

    EXPECT_EQ(expectedString, actualString);
}
