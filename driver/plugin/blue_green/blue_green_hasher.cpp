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

#include "blue_green_hasher.h"

static constexpr int HASH_MULTIPLIER = 31;

int32_t BlueGreenHasher::GetHash(const std::string& str) {
    int32_t hash = 0;
    for (const char& c : str) {
        hash = (HASH_MULTIPLIER * hash) + static_cast<int>(c);
    }
    return hash;
}
