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

#ifndef SLIDING_CACHE_MAP_H_
#define SLIDING_CACHE_MAP_H_

#include <chrono>
#include <mutex>
#include <map>

template <typename V>
struct CacheEntry {
    V value;
    std::chrono::steady_clock::time_point expiry;
    std::chrono::milliseconds time_to_expire_ms;
};

template <typename K, typename V>
class SlidingCacheMap {
public:
    SlidingCacheMap() = default;
    ~SlidingCacheMap() = default;

    void Put(const K& key, const V& value) {
        Put(key, value, DEFAULT_EXPIRATION_MS);
    };

    void Put(const K& key, const V& value, std::chrono::milliseconds ms_ttl) {
        const std::lock_guard<std::mutex> lock(cache_lock);
        const std::chrono::steady_clock::time_point expiry_time =
            std::chrono::steady_clock::now() + ms_ttl;
        cache[key] = CacheEntry{value, expiry_time, ms_ttl};
    }

    void PutIfAbsent(const K& key, const V& value) {
        PutIfAbsent(key, value, DEFAULT_EXPIRATION_MS);
    }

    void PutIfAbsent(const K& key, const V& value, std::chrono::milliseconds ms_ttl) {
        const std::lock_guard<std::mutex> lock(cache_lock);
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (auto itr = cache.find(key); itr != cache.end()) {
            CacheEntry<V> & entry = itr->second;
            // Already in cache & is not expired
            if (entry.expiry > now) {
                // Update TTL & Return value
                entry.expiry = now + entry.time_to_expire_ms;
                return;
            }
        }
        // Either not in cache or is expired, put new into cache
        const std::chrono::steady_clock::time_point expiry_time =
            std::chrono::steady_clock::now() + ms_ttl;
        cache[key] = CacheEntry{value, expiry_time, ms_ttl};
    }

    V Get(const K& key) {
        const std::lock_guard<std::mutex> lock(cache_lock);
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (auto itr = cache.find(key); itr != cache.end()) {
            CacheEntry<V> & entry = itr->second;
            if (entry.expiry > now) {
                // Update TTL & Return value
                entry.expiry = now + entry.time_to_expire_ms;
                return entry.value;
            }
            // Expired, remove from cache
            cache.erase(itr);
        }
        return {};
    }

    bool Find(const K& key) {
        const std::lock_guard<std::mutex> lock(cache_lock);
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (auto itr = cache.find(key); itr != cache.end()) {
            CacheEntry<V> & entry = itr->second;
            if (entry.expiry > now) {
                // Update TTL & Return found
                entry.expiry = now + entry.time_to_expire_ms;
                return true;
            }
            // Expired, remove from cache
            cache.erase(itr);
        }
        return false;
    }

    unsigned int Size() {
        const std::lock_guard<std::mutex> lock(cache_lock);
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        for (auto itr = cache.begin(); itr != cache.end();) {
            if (itr->second.expiry < now) {
                itr = cache.erase(itr);
            } else {
                ++itr;
            }
        }
        return cache.size();
    }

    void Clear() {
        const std::lock_guard<std::mutex> lock(cache_lock);
        cache.clear();
    }

    void Delete(const K& key) {
        const std::lock_guard<std::mutex> lock(cache_lock);
        cache.erase(key);
    }

private:
    static inline const std::chrono::milliseconds
        DEFAULT_EXPIRATION_MS = std::chrono::minutes(15);
    std::map<K, CacheEntry<V>> cache;
    mutable std::mutex cache_lock;
};

#endif // SLIDING_CACHE_MAP_H_
