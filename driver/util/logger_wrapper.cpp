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

#include "logger_wrapper.h"

#include <ng-log/logging.h>

#ifdef WIN32
    #include <windows.h>
#else
    #include <iconv.h>
#endif

#include "rds_strings.h"

FileSink::FileSink(const std::string log_file_path, int log_threshold)
{
    std::filesystem::path log_dir(log_file_path);
    if (log_dir.is_relative()) {
        log_dir = std::filesystem::current_path().append(log_file_path);
    }

    if (!std::filesystem::exists(log_dir)) {
        std::filesystem::create_directories(log_dir);
    }
    log_dir.append(logger_config::PROGRAM_NAME + logger_config::LOG_SUFFIX);
    log_file.open(log_dir, std::ofstream::out | std::ofstream::app);
    threshold = log_threshold;
}

FileSink::~FileSink()
{
    if (log_file.is_open()) {
        log_file.flush();
        log_file.close();
    }
}

void FileSink::send(nglog::LogSeverity severity, const char* /*full_filename*/,
    const char* base_filename, int line,
    const nglog::LogMessageTime& log_time, const char* message,
    std::size_t message_len)
{
    if (severity <= threshold && log_file.is_open()) {
        log_file << nglog::GetLogSeverityName(severity) << ' ' << log_time.when() << ' ' << base_filename << ':' << line << ' ';
        std::copy_n(message, message_len, std::ostreambuf_iterator<char>{log_file});
        log_file << '\n';

        log_file.flush();
    }
}

void LoggerWrapper::Initialize()
{
    Initialize(logger_config::DEFAULT_LOG_LOCATION, logger_config::DEFAULT_LOG_THRESHOLD);
}

void LoggerWrapper::Initialize(int threshold)
{
    Initialize(logger_config::DEFAULT_LOG_LOCATION, threshold);
}

void LoggerWrapper::Initialize(RDS_STR log_location)
{
    Initialize(log_location, logger_config::DEFAULT_LOG_THRESHOLD);
}

void LoggerWrapper::Initialize(RDS_STR log_location, int threshold)
{
    if (0 == logger_init_count++) {
        // Set to 4 to disable console output
        threshold = threshold >= 0 ? threshold : logger_config::DEFAULT_LOG_THRESHOLD;
        FLAGS_stderrthreshold = threshold;
        FLAGS_timestamp_in_logfile_name = false;
        if (log_location.empty()) {
            log_location = logger_config::DEFAULT_LOG_LOCATION;
        }
        SetLogDirectory(log_location);
        nglog::InitializeLogging(logger_config::PROGRAM_NAME.c_str());
    }
}

void LoggerWrapper::Shutdown()
{
    if (--logger_init_count == 0) {
        nglog::ShutdownLogging();
    }
}

void LoggerWrapper::SetLogDirectory(const RDS_STR &directory_path)
{
    std::filesystem::path log_dir(directory_path);
    if (!std::filesystem::exists(log_dir)) {
        std::filesystem::create_directories(log_dir);
    }
    FLAGS_log_dir = ToStr(directory_path);
}
