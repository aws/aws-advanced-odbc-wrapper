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

#include <mutex>

#include "logger_wrapper.h"

Aws::SDKOptions AwsSdkHelper::sdk_options;
std::atomic<bool> AwsSdkHelper::sdk_initialized{false};

void AwsSdkHelper::EnsureInitialized()
{
    static std::once_flag init_flag;
    std::call_once(init_flag, [] {
        Aws::InitAPI(sdk_options);
        sdk_initialized.store(true);
        LOG(INFO) << "Initialized AWS SDK Instance";
    });
}

void AwsSdkHelper::PerformShutdown()
{
    bool expected = true;
    if (sdk_initialized.compare_exchange_strong(expected, false)) {
        Aws::ShutdownAPI(sdk_options);
        LOG(INFO) << "Shut down AWS SDK Instance";
    }
}
