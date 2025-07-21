#include "aws_sdk_helper.h"

Aws::SDKOptions AwsSdkHelper::sdk_options;
std::atomic<int> AwsSdkHelper::sdk_reference_count{0};
std::mutex AwsSdkHelper::sdk_mutex;

void AwsSdkHelper::Init()
{
    std::lock_guard<std::mutex> lock(sdk_mutex);
    if (1 == ++sdk_reference_count) {
        Aws::InitAPI(sdk_options);
    }
}

void AwsSdkHelper::Shutdown()
{
    std::lock_guard<std::mutex> lock(sdk_mutex);
    if (0 == --sdk_reference_count) {
        Aws::ShutdownAPI(sdk_options);
    }
}
