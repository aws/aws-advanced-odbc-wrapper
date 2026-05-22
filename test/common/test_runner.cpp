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

#include <iostream>
#include "gtest/gtest.h"

class FlushingListener : public testing::EmptyTestEventListener {
public:
    void OnTestStart(const testing::TestInfo& info) override {
        std::cout << "[ RUN      ] "
                  << info.test_suite_name() << "." << info.name()
                  << std::endl;
        std::cout.flush();
        std::cerr.flush();
    }

    void OnTestPartResult(const testing::TestPartResult& result) override {
        if (result.passed() || result.skipped()) {
            return;
        }
        const char* file = result.file_name() ? result.file_name() : "unknown";
        const int line = result.line_number();
        std::cout << file << ":" << line << ": Failure" << std::endl;
        std::cout << result.message() << std::endl;
        std::cout.flush();
        std::cerr.flush();
    }

    void OnTestEnd(const testing::TestInfo& info) override {
        const char* status = info.result()->Passed() ? "[       OK ]" : "[  FAILED  ]";
        std::cout << status << " "
                  << info.test_suite_name() << "." << info.name()
                  << " (" << info.result()->elapsed_time() << " ms)"
                  << std::endl;
        std::cout.flush();
        std::cerr.flush();
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    testing::TestEventListeners& listeners = testing::UnitTest::GetInstance()->listeners();
    delete listeners.Release(listeners.default_result_printer());
    listeners.Append(new FlushingListener());

    int failures = RUN_ALL_TESTS();
    std::cout << (failures ? "Not all tests passed." : "All tests passed") << std::endl;
    return failures;
}
