# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project("aws-advanced-odbc-wrapper")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configuration ----------------------------------------------------------------------------------------------
set(BUILD_ANSI OFF CACHE BOOL "Toggle to build with Unicode Wrapper")
set(BUILD_UNICODE OFF CACHE BOOL "Toggle to build with Ansi Wrapper")
set(BUILD_UNIT_TEST OFF CACHE BOOL "Toggle to build Unit Tests")
set(WITH_IODBC OFF CACHE BOOL "Build with iODBC")

# External Tools ---------------------------------------------------------------------------------------------
include(FetchContent)

# Fetch External Libraries  ----------------------------------------------------------------------------------
# -- AWS SDK --
if(UNIX)
    message("Unix - Adding ZLIB explicitly for AWS SDK")
    find_package(ZLIB REQUIRED)
endif()

# Path to built AWS SDK
set(AWS_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/aws_sdk/install")
list(APPEND CMAKE_PREFIX_PATH ${AWS_SDK_DIR})

find_package(AWSSDK REQUIRED COMPONENTS core rds secretsmanager sts)
list(APPEND EXTERNAL_LIBRARIES ${AWSSDK_LINK_LIBRARIES})

# -- ODBC --
if(APPLE)
    message("Apple to use unixODBC")
    set(ODBC_ROOT /opt/homebrew/opt/unixodbc/)
    set(ODBC_INCLUDE_DIR /opt/homebrew/opt/unixodbc/include)
endif()

find_package(ODBC REQUIRED)
list(APPEND EXTERNAL_LIBRARIES ODBC::ODBC)

# ODBC Inst
if(WIN32)
    list(APPEND EXTERNAL_LIBRARIES odbccp32 legacy_stdio_definitions)
else(WIN32)
    # TODO - Test iODBC
    if(WITH_IODBC)
        find_library(ODBC_INST_LIB
            NAME
                iodbcinst
            HINTS
                ODBC_ROOT
            REQUIRED
        )
    else() # unixODBC
        find_library(ODBC_INST_LIB
            NAME
                odbcinst
            HINTS
                ODBC_ROOT
            REQUIRED
        )
        add_compile_definitions(
            SQL_WCHART_CONVERT # Use `wchar_t` instead of `unsigned short`
        )
    endif()
    list(APPEND EXTERNAL_LIBRARIES ${ODBC_INST_LIB})
endif()

# -- NG-LOG --
FetchContent_Declare(
    ng-log
    GIT_REPOSITORY https://github.com/ng-log/ng-log.git
    GIT_TAG        v0.8.1
)
set(BUILD_TESTING OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(ng-log)
list(APPEND EXTERNAL_LIBRARIES ng-log::ng-log)

# Build Driver -----------------------------------------------------------------------------------------------
add_subdirectory(driver)

# Build Unit Tests -------------------------------------------------------------------------------------------
if(UNIT_TEST)
    add_subdirectory(test/unit_test)
endif()
