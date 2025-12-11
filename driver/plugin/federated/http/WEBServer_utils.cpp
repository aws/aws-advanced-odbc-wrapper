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

#include "WEBServer_utils.h"

int WebServerUtils::GenerateRandomInteger(int low, int high)
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> dist(low, high);

    return dist(generator);
}

std::string WebServerUtils::GenerateState()
{
    const char state_char_list[37] = "0123456789abcdefghijklmnopqrstuvwxyz";
    const int chars_size = (sizeof(state_char_list) / sizeof(*state_char_list)) - 1;
    const int rand_size = GenerateRandomInteger(9, chars_size - 1);
    std::string state;

    state.reserve(rand_size);

    for (int i = 0; i < rand_size; ++i) {
        state.push_back(state_char_list[GenerateRandomInteger(0, rand_size)]);
    }

    return state;
}
