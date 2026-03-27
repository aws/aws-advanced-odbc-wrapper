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



#ifndef CONCURRENT_STACK_H
#define CONCURRENT_STACK_H

#include <functional>
#include <mutex>
#include <vector>

template <typename Value>
class ConcurrentStack {
public:
    ConcurrentStack() = default;
    ~ConcurrentStack() = default;

    Value Back() {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.back();
    }

    void PushBack(const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.push_back(value);
    }

    void PopBack() {
        std::lock_guard<std::mutex> lock(mutex_);
        stack_.pop_back();
    }

    Value PopAndGetBack() {
        std::lock_guard<std::mutex> lock(mutex_);
        Value v = stack_.back();
        stack_.pop_back();
        return v;
    }

    size_t Size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.size();
    }

    bool Empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.empty();
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        stack_.clear();
    }

    void ForEach(std::function<void(Value)> fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& val : stack_) {
            fn(val);
        }
    }

    void RemoveIf(std::function<bool(Value)> fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        stack_.erase(std::remove_if(stack_.begin(), stack_.end(), fn), stack_.end());
    }

private:
    std::vector<Value> stack_;
    mutable std::mutex mutex_;
};

#endif  // CONCURRENT_STACK_H
