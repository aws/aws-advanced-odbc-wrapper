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

#ifndef CONCURRENT_MAP_H_
#define CONCURRENT_MAP_H_

#include <map>
#include <mutex>
#include <shared_mutex>

template <typename Key, typename Value>
class ConcurrentMap {
public:
    ConcurrentMap() = default;
    ~ConcurrentMap() = default;

    ConcurrentMap(const ConcurrentMap& other) {
        std::shared_lock<std::shared_mutex> other_lock(other.mutex_);
        map_ = other.map_;
    };

    ConcurrentMap& operator=(const ConcurrentMap& other) {
        if (this == &other) {
            return *this;
        }

        std::scoped_lock<std::shared_mutex, std::shared_mutex> lock(mutex_, other.mutex_);
        map_ = other.map_;

        return *this;
    };

    void InsertOrAssign(const Key& key, const Value& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        map_.insert_or_assign(key, value);
    };

    bool TryEmplace(const Key& key, const Value& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto itr_pair = map_.try_emplace(key, value);
        return itr_pair.second;
    };

    void CopyMap(std::map<Key, Value> other) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        map_.insert(other.begin(), other.end());
    };

    Value Get(const Key& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (auto itr = map_.find(key); itr != map_.end()) {
            return itr->second;
        }
        return {};
    };

    std::map<Key, Value> GetMapCopy() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::map<Key, Value> map_copy;
        map_copy.insert(map_.begin(), map_.end());
        return map_copy;
    };

    void Erase(const Key& key) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        map_.erase(key);
    };

    bool Contains(const Key& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return map_.contains(key);
    };

    size_t Size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return map_.size();
    };

    void Clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        map_.clear();
    };

    bool operator==(const ConcurrentMap& other) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::shared_lock<std::shared_mutex> other_lock(other.mutex_);
        return map_ == other.map_;
    };

private:
    std::map<Key, Value> map_;
    mutable std::shared_mutex mutex_;
};

#endif // CONCURRENT_MAP_H_
