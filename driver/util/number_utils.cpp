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

#include "number_utils.h"

#include <cctype>
#include <charconv>
#include <system_error>

namespace {
    template <typename T>
    std::optional<T> Parse(const std::string &str) {
        // Tolerate surrounding whitespace
        const char* begin = str.data();
        const char* end = begin + str.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
            ++begin;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
            --end;
        }
        // Tolerate explicit '+' sign
        if (begin + 1 < end && *begin == '+' && std::isdigit(static_cast<unsigned char>(*(begin + 1))) != 0) {
            ++begin;
        }

        T value{};
        const std::from_chars_result result = std::from_chars(begin, end, value, 10);
        if (begin == end || result.ec != std::errc() || result.ptr != end) {
            return std::nullopt;
        }
        return value;
    }
}  // namespace

std::optional<int> NumberUtils::ParseInt(const std::string &str) {
    return Parse<int>(str);
}

std::optional<int64_t> NumberUtils::ParseInt64(const std::string &str) {
    return Parse<int64_t>(str);
}
