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

#ifndef AWS_SDK_HELPER_H
#define AWS_SDK_HELPER_H

#include <aws/core/Aws.h>

#include <atomic>
#include <mutex>

class AwsSdkHelper {
public:
    AwsSdkHelper() = default;
    static void Init();
    static void Shutdown();

    // Prevent copy constructors
    AwsSdkHelper(const AwsSdkHelper&) = delete;
    AwsSdkHelper(AwsSdkHelper&&) = delete;
    AwsSdkHelper& operator=(const AwsSdkHelper&) = delete;
    AwsSdkHelper& operator=(AwsSdkHelper&&) = delete;

private:
    static Aws::SDKOptions sdk_options;
    static std::atomic<int> sdk_reference_count;
    static std::mutex sdk_mutex;
};

#endif // AWS_SDK_HELPER_H
