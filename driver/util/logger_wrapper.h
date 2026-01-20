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

#ifndef LOGGER_WRAPPER_H_
#define LOGGER_WRAPPER_H_

#ifdef XCODE_BUILD
// Setting this to a value greater than 3 strips out all of the
#define NGLOG_STRIP_LOG 4
#endif /* XCODE_BUILD */

#include <ng-log/logging.h>

#include <atomic>
#include <filesystem>
#include <mutex>

namespace logger_config {
    const std::string PROGRAM_NAME = "aws-odbc-wrapper";
    const std::string DEFAULT_LOG_LOCATION = std::filesystem::temp_directory_path().append(PROGRAM_NAME).string();
    const int DEFAULT_LOG_THRESHOLD = 4;
}  // namespace logger_config

class LoggerWrapper {
public:
    LoggerWrapper();
    LoggerWrapper(int threshold);
    LoggerWrapper(const std::string &log_location);
    LoggerWrapper(std::string log_location, int threshold);
    ~LoggerWrapper();

    // Prevent copy constructors
    LoggerWrapper(const LoggerWrapper&) = delete;
    LoggerWrapper(LoggerWrapper&&) = delete;
    LoggerWrapper& operator=(const LoggerWrapper&) = delete;
    LoggerWrapper& operator=(LoggerWrapper&&) = delete;

private:
    static void SetLogDirectory(const std::string &directory_path);
    static inline std::atomic<int> logger_init_count_{0};
    static std::mutex logger_mutex_;
};

#endif // LOGGER_WRAPPER_H_
