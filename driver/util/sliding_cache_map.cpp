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

#include "sliding_cache_map.h"

#include <chrono>
#include <mutex>

#include "plugin_service.h"

#include "../host_info.h"
#include "../host_list_providers/cluster_topology_monitor.h"
#include "../host_selector/round_robin_host_selector.h"
#include "../plugin/limitless/limitless_router_monitor.h"

template <typename K, typename V>
void SlidingCacheMap<K, V>::Put(const K& key, const V& value) {
    Put(key, value, DEFAULT_EXPIRATION_MS);
}

template <typename K, typename V>
void SlidingCacheMap<K, V>::Put(const K& key, const V& value, std::chrono::milliseconds ms_ttl) {
    const std::lock_guard<std::mutex> lock(cache_lock);
    const std::chrono::steady_clock::time_point expiry_time =
        std::chrono::steady_clock::now() + ms_ttl;
    cache[key] = CacheEntry{value, expiry_time, ms_ttl};
}

template <typename K, typename V>
void SlidingCacheMap<K, V>::PutIfAbsent(const K &key, const V &value) {
    PutIfAbsent(key, value, DEFAULT_EXPIRATION_MS);
}

template <typename K, typename V>
void SlidingCacheMap<K, V>::PutIfAbsent(const K &key, const V &value, std::chrono::milliseconds ms_ttl) {
    const std::lock_guard<std::mutex> lock(cache_lock);
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (auto itr = cache.find(key); itr != cache.end()) {
        CacheEntry& entry = itr->second;
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

template <typename K, typename V>
V SlidingCacheMap<K, V>::Get(const K& key) {
    const std::lock_guard<std::mutex> lock(cache_lock);
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (auto itr = cache.find(key); itr != cache.end()) {
        CacheEntry& entry = itr->second;
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

template <typename K, typename V>
bool SlidingCacheMap<K, V>::Find(const K& key) {
    const std::lock_guard<std::mutex> lock(cache_lock);
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (auto itr = cache.find(key); itr != cache.end()) {
        CacheEntry& entry = itr->second;
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

template <typename K, typename V>
unsigned int SlidingCacheMap<K, V>::Size() {
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

template <typename K, typename V>
void SlidingCacheMap<K, V>::Clear() {
    const std::lock_guard<std::mutex> lock(cache_lock);
    cache.clear();
}

template <typename K, typename V>
void SlidingCacheMap<K, V>::Delete(const K& key) {
    const std::lock_guard<std::mutex> lock(cache_lock);
    cache.erase(key);
}

// Explicit Template Instantiations
template class SlidingCacheMap<std::string, std::shared_ptr<round_robin_property::RoundRobinClusterInfo>>;
template class SlidingCacheMap<std::string, std::string>;
template class SlidingCacheMap<std::string, std::vector<HostInfo>>;
template class SlidingCacheMap<std::string, std::pair<unsigned int, std::shared_ptr<LimitlessRouterMonitor>>>;
template class SlidingCacheMap<std::string, std::shared_ptr<ClusterTopologyMonitor>>;
template class SlidingCacheMap<std::string, HostFilter>;
