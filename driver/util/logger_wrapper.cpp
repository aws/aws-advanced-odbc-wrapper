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

void LoggerWrapper::Initialize()
{
    Initialize(logger_config::DEFAULT_LOG_LOCATION, logger_config::DEFAULT_LOG_THRESHOLD);
}

void LoggerWrapper::Initialize(int threshold)
{
    Initialize(logger_config::DEFAULT_LOG_LOCATION, threshold);
}

void LoggerWrapper::Initialize(std::string log_location)
{
    Initialize(std::move(log_location), logger_config::DEFAULT_LOG_THRESHOLD);
}

void LoggerWrapper::Initialize(std::string log_location, int threshold)
{
    if (0 == logger_init_count++) {
        // Set to 4 to disable console output
        threshold = threshold >= 0 ? threshold : logger_config::DEFAULT_LOG_THRESHOLD;
        FLAGS_stderrthreshold = threshold;
        // Also adds PID to file, needed for multi-process safety
        FLAGS_timestamp_in_logfile_name = true;
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

void LoggerWrapper::SetLogDirectory(const std::string &directory_path)
{
    const std::filesystem::path log_dir(directory_path);
    if (!std::filesystem::exists(log_dir)) {
        std::filesystem::create_directories(log_dir);
    }
    FLAGS_log_dir = directory_path;
}
