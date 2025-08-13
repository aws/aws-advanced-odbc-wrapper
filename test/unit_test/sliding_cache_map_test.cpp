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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
    const std::string cache_key_a("key_a");
    const std::string cache_key_b("key_b");
    const std::string cache_val_a("val_a");
    const std::string cache_val_b("val_b");
    const std::string cache_empty("");
    const std::chrono::milliseconds cache_exp_short(std::chrono::seconds(3));
    const std::chrono::milliseconds cache_exp_mid(std::chrono::seconds(5));
    const std::chrono::milliseconds cache_exp_long(std::chrono::seconds(6000));
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
    cache.Put(cache_key_a, cache_val_a);
    cache.Put(cache_key_b, cache_val_b, cache_exp_long);
    EXPECT_STREQ(cache_val_a.c_str(), cache.Get(cache_key_a).c_str());
    EXPECT_STREQ(cache_val_b.c_str(), cache.Get(cache_key_b).c_str());
}

TEST_F(SlidingCacheMapTest, put_cache_update) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(cache_key_a, cache_val_a);
    cache.Put(cache_key_b, cache_val_b, cache_exp_short);
    EXPECT_EQ(2, cache.Size());
    EXPECT_STREQ(cache_val_b.c_str(), cache.Get(cache_key_b).c_str());

    cache.Put(cache_key_b, cache_val_b, cache_exp_long);
    std::this_thread::sleep_for(cache_exp_mid);
    EXPECT_EQ(2, cache.Size());
    EXPECT_STREQ(cache_val_b.c_str(), cache.Get(cache_key_b).c_str());
}

TEST_F(SlidingCacheMapTest, get_cache_hit) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(cache_key_a, cache_val_a);
    EXPECT_STREQ(cache_val_a.c_str(), cache.Get(cache_key_a).c_str());
}

TEST_F(SlidingCacheMapTest, get_cache_miss) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_STREQ(cache_empty.c_str(), cache.Get(cache_key_a).c_str());
}

TEST_F(SlidingCacheMapTest, get_cache_expire) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(cache_key_a, cache_val_a, cache_exp_short);
    std::this_thread::sleep_for(cache_exp_mid);
    EXPECT_STREQ(cache_empty.c_str(), cache.Get(cache_key_a).c_str());
}

TEST_F(SlidingCacheMapTest, find_cache_miss) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_FALSE(cache.Find("Missing"));
}

TEST_F(SlidingCacheMapTest, find_cache_hit) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(cache_key_a, cache_val_a);
    EXPECT_TRUE(cache.Find(cache_key_a));
}

TEST_F(SlidingCacheMapTest, cache_size) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(cache_key_a, cache_val_a);
    cache.Put(cache_key_b, cache_val_b);
    EXPECT_EQ(2, cache.Size());
}

TEST_F(SlidingCacheMapTest, cache_size_expire) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(cache_key_a, cache_val_a, cache_exp_short);
    cache.Put(cache_key_b, cache_val_b, cache_exp_short);
    EXPECT_EQ(2, cache.Size());
    std::this_thread::sleep_for(cache_exp_mid);
    EXPECT_EQ(0, cache.Size());
}

TEST_F(SlidingCacheMapTest, cache_clear) {
    SlidingCacheMap<std::string, std::string> cache;
    cache.Put(cache_key_a, cache_val_a);
    cache.Put(cache_key_b, cache_val_b);
    EXPECT_EQ(2, cache.Size());
    cache.Clear();
    EXPECT_EQ(0, cache.Size());
}

TEST_F(SlidingCacheMapTest, ttl_update_find) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(cache_key_a, cache_val_a, cache_exp_mid);
    EXPECT_EQ(1, cache.Size());

    std::this_thread::sleep_for(cache_exp_short);

    // Touch to refresh TTL
    EXPECT_TRUE(cache.Find(cache_key_a));

    // Sleep again combined with previous to expire original put
    std::this_thread::sleep_for(cache_exp_short);

    // Should be valid due to earlier `find()` updates TTL
    EXPECT_EQ(1, cache.Size());
    EXPECT_STREQ(cache_val_a.c_str(), cache.Get(cache_key_a).c_str());
}

TEST_F(SlidingCacheMapTest, ttl_update_get) {
    SlidingCacheMap<std::string, std::string> cache;
    EXPECT_EQ(0, cache.Size());
    cache.Put(cache_key_a, cache_val_a, cache_exp_mid);
    EXPECT_EQ(1, cache.Size());

    std::this_thread::sleep_for(cache_exp_short);

    // Touch to refresh TTL
    EXPECT_STREQ(cache_val_a.c_str(), cache.Get(cache_key_a).c_str());

    // Sleep again combined with previous to expire original put
    std::this_thread::sleep_for(cache_exp_short);

    // Should be valid due to earlier `get()` updates TTL
    EXPECT_EQ(1, cache.Size());
    EXPECT_STREQ(cache_val_a.c_str(), cache.Get(cache_key_a).c_str());
}
