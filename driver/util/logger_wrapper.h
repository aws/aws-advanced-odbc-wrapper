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
// Google logging functionality
#define NGLOG_STRIP_LOG 4
#endif /* XCODE_BUILD */

#include <ng-log/logging.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "rds_strings.h"

namespace logger_config {
    const std::string PROGRAM_NAME = "aws-odbc-wrapper";
    const std::string LOG_SUFFIX = ".log";
#ifdef UNICODE
    const RDS_STR DEFAULT_LOG_LOCATION = std::filesystem::temp_directory_path().append(PROGRAM_NAME).wstring();
#else
    const RDS_STR DEFAULT_LOG_LOCATION = std::filesystem::temp_directory_path().append(PROGRAM_NAME).string();
#endif
    const int DEFAULT_LOG_THRESHOLD = 4;
}  // namespace logger_config

struct FileSink : nglog::LogSink {
    FileSink(const std::string log_file_path, int log_threshold);
    ~FileSink();

    virtual void send(nglog::LogSeverity severity, const char* /*full_filename*/,
        const char* base_filename, int line,
        const nglog::LogMessageTime& log_time, const char* message,
        std::size_t message_len) override;
    std::ofstream log_file;
    int threshold = 0;
};

class LoggerWrapper {
public:
    LoggerWrapper() = default;

    static void Initialize();
    static void Initialize(int threshold);
    static void Initialize(RDS_STR log_location);
    static void Initialize(RDS_STR log_location, int threshold);

    static void Shutdown();

    // Prevent copy constructors
    LoggerWrapper(const LoggerWrapper&) = delete;
    LoggerWrapper(LoggerWrapper&&) = delete;
    LoggerWrapper& operator=(const LoggerWrapper&) = delete;
    LoggerWrapper& operator=(LoggerWrapper&&) = delete;

private:
    static void SetLogDirectory(const RDS_STR &directory_path);
    static inline std::atomic<int> logger_init_count = 0;
};

#endif // LOGGER_WRAPPER_H_
