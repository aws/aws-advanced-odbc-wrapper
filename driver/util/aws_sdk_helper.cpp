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

#include "aws_sdk_helper.h"

Aws::SDKOptions AwsSdkHelper::sdk_options;
std::atomic<int> AwsSdkHelper::sdk_reference_count{0};
std::mutex AwsSdkHelper::sdk_mutex;

void AwsSdkHelper::Init()
{
    const std::lock_guard<std::mutex> lock(sdk_mutex);
    if (1 == ++sdk_reference_count) {
        Aws::InitAPI(sdk_options);
    }
}

void AwsSdkHelper::Shutdown()
{
    const std::lock_guard<std::mutex> lock(sdk_mutex);
    if (0 == --sdk_reference_count) {
        Aws::ShutdownAPI(sdk_options);
    }
}
