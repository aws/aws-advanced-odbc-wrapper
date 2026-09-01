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

#include "../../driver/util/sliding_cache_map.h"

#include <string>
#include <thread>
#include <chrono>
#include <optional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
    const std::string CACHE_KEY_A("key_a");
    const std::string CACHE_KEY_B("key_b");
    const std::string CACHE_VAL_A("val_a");
    const std::string CACHE_VAL_B("val_b");
    const std::string CACHE_EMPTY("");
    const std::chrono::milliseconds CACHE_EXP_SHORT(std::chrono::seconds(3));
    const std::chrono::milliseconds CACHE_EXP_MID(std::chrono::seconds(5));
    const std::chrono::milliseconds CACHE_EXP_LONG(std::chrono::seconds(6000));
}

class SlidingCacheMapTest : public testing::Test {
  protected:
    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    // Runs per test case
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SlidingCacheMapTest, put_cache) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(CACHE_KEY_A, CACHE_VAL_A);
    cache.Put(CACHE_KEY_B, CACHE_VAL_B, CACHE_EXP_LONG);
    EXPECT_EQ(CACHE_VAL_A, cache.Get(CACHE_KEY_A));
    EXPECT_EQ(CACHE_VAL_B, cache.Get(CACHE_KEY_B));
}

TEST_F(SlidingCacheMapTest, put_cache_update) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(CACHE_KEY_A, CACHE_VAL_A);
    cache.Put(CACHE_KEY_B, CACHE_VAL_B, CACHE_EXP_SHORT);
    EXPECT_EQ(2, cache.Size());
    EXPECT_EQ(CACHE_VAL_B, cache.Get(CACHE_KEY_B));

    cache.Put(CACHE_KEY_B, CACHE_VAL_B, CACHE_EXP_LONG);
    std::this_thread::sleep_for(CACHE_EXP_MID);
    EXPECT_EQ(2, cache.Size());
    EXPECT_EQ(CACHE_VAL_B, cache.Get(CACHE_KEY_B));
}

TEST_F(SlidingCacheMapTest, get_cache_hit) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(CACHE_KEY_A, CACHE_VAL_A);
    EXPECT_EQ(CACHE_VAL_A, cache.Get(CACHE_KEY_A));
}

TEST_F(SlidingCacheMapTest, get_cache_miss) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(std::nullopt, cache.Get(CACHE_KEY_A));
}

TEST_F(SlidingCacheMapTest, get_stored_default_value_is_a_hit) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(CACHE_KEY_A, CACHE_EMPTY);
    // A stored default-constructed value is distinguishable from a miss
    EXPECT_EQ(CACHE_EMPTY, cache.Get(CACHE_KEY_A));
    EXPECT_EQ(std::nullopt, cache.Get(CACHE_KEY_B));
}

TEST_F(SlidingCacheMapTest, get_cache_expire) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(CACHE_KEY_A, CACHE_VAL_A, CACHE_EXP_SHORT);
    std::this_thread::sleep_for(CACHE_EXP_MID);
    EXPECT_EQ(std::nullopt, cache.Get(CACHE_KEY_A));
}

TEST_F(SlidingCacheMapTest, find_cache_miss) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_FALSE(cache.Find("Missing"));
}

TEST_F(SlidingCacheMapTest, find_cache_hit) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(CACHE_KEY_A, CACHE_VAL_A);
    EXPECT_TRUE(cache.Find(CACHE_KEY_A));
}

TEST_F(SlidingCacheMapTest, cache_size) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(CACHE_KEY_A, CACHE_VAL_A);
    cache.Put(CACHE_KEY_B, CACHE_VAL_B);
    EXPECT_EQ(2, cache.Size());
}

TEST_F(SlidingCacheMapTest, cache_size_expire) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(CACHE_KEY_A, CACHE_VAL_A, CACHE_EXP_SHORT);
    cache.Put(CACHE_KEY_B, CACHE_VAL_B, CACHE_EXP_SHORT);
    EXPECT_EQ(2, cache.Size());
    std::this_thread::sleep_for(CACHE_EXP_MID);
    EXPECT_EQ(0, cache.Size());
}

TEST_F(SlidingCacheMapTest, cache_clear) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(CACHE_KEY_A, CACHE_VAL_A);
    cache.Put(CACHE_KEY_B, CACHE_VAL_B);
    EXPECT_EQ(2, cache.Size());
    cache.Clear();
    EXPECT_EQ(0, cache.Size());
}

TEST_F(SlidingCacheMapTest, ttl_update_find) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(CACHE_KEY_A, CACHE_VAL_A, CACHE_EXP_MID);
    EXPECT_EQ(1, cache.Size());

    std::this_thread::sleep_for(CACHE_EXP_SHORT);

    // Touch to refresh TTL
    EXPECT_TRUE(cache.Find(CACHE_KEY_A));

    // Sleep again combined with previous to expire original put
    std::this_thread::sleep_for(CACHE_EXP_SHORT);

    // Should be valid due to earlier `find()` updates TTL
    EXPECT_EQ(1, cache.Size());
    EXPECT_EQ(CACHE_VAL_A, cache.Get(CACHE_KEY_A));
}

TEST_F(SlidingCacheMapTest, ttl_update_get) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(CACHE_KEY_A, CACHE_VAL_A, CACHE_EXP_MID);
    EXPECT_EQ(1, cache.Size());

    std::this_thread::sleep_for(CACHE_EXP_SHORT);

    // Touch to refresh TTL
    EXPECT_EQ(CACHE_VAL_A, cache.Get(CACHE_KEY_A));

    // Sleep again combined with previous to expire original put
    std::this_thread::sleep_for(CACHE_EXP_SHORT);

    // Should be valid due to earlier `get()` updates TTL
    EXPECT_EQ(1, cache.Size());
    EXPECT_EQ(CACHE_VAL_A, cache.Get(CACHE_KEY_A));
}
