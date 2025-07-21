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
