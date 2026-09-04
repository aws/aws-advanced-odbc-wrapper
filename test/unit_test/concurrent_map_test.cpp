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

#include "../../driver/util/concurrent_map.h"

#include <optional>
#include <string>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
    const std::string CACHE_KEY_A("key_a");
    const std::string CACHE_KEY_B("key_b");
}

class ConcurrentMapTest : public testing::Test {
public:
    void LoadThread(ConcurrentMap<std::string, int> map, std::string key) {

    }
protected:
    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    // Runs per test case
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConcurrentMapTest, sync_basic_operations) {
    ConcurrentMap<std::string, int> cache;
    // Insert & Get
    cache.InsertOrAssign(CACHE_KEY_A, 1);
    EXPECT_EQ(1, cache.Get(CACHE_KEY_A));

    // Update & Get
    cache.InsertOrAssign(CACHE_KEY_A, 2);
    EXPECT_EQ(2, cache.Get(CACHE_KEY_A));

    // Erase
    cache.Erase(CACHE_KEY_A);
    EXPECT_FALSE(cache.Contains(CACHE_KEY_A));
    EXPECT_EQ(std::nullopt, cache.Get(CACHE_KEY_A));

    // A stored default-constructed value is distinguishable from a miss
    cache.InsertOrAssign(CACHE_KEY_A, 0);
    EXPECT_EQ(0, cache.Get(CACHE_KEY_A));
    cache.Erase(CACHE_KEY_A);

    // Size
    EXPECT_EQ(0, cache.Size());
    cache.InsertOrAssign(CACHE_KEY_A, 1);
    EXPECT_EQ(1, cache.Size());
    cache.InsertOrAssign(CACHE_KEY_B, 2);
    EXPECT_EQ(2, cache.Size());

    // Clear
    cache.Clear();
    EXPECT_EQ(0, cache.Size());

    // Equality
    ConcurrentMap<std::string, int> cache_b;
    EXPECT_TRUE(cache == cache_b);
    cache_b.InsertOrAssign(CACHE_KEY_B, 2);
    EXPECT_FALSE(cache == cache_b);
}
