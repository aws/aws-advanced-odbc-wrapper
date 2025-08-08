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
project("unit-test")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources ----------------------------------------------------------------------------------------------------
set(TEST_UTILITIES
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/connection_string_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_objects.h
)

set(TEST_SUITE
    ${CMAKE_CURRENT_SOURCE_DIR}/adfs_auth_plugin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adfs_saml_util_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/auth_provider_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iam_auth_plugin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/okta_auth_plugin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/okta_saml_util_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/secrets_manager_plugin_test.cpp
)

set(TEST_RUNNER
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/test_runner.cpp
)

# Configure Build Defines ------------------------------------------------------------------------------------
if(UNICODE)
    SET(WRAPPER_NAME "aws-advanced-odbc-wrapper-w")
else()
    SET(WRAPPER_NAME "aws-advanced-odbc-wrapper-a")
endif()

# Fetch External Libraries ----------------------------------------------------------------------------------
include(FetchContent)
# -- Google Test --
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
list(APPEND EXTERNAL_LIBRARIES gtest_main)
list(APPEND EXTERNAL_LIBRARIES gmock_main)

# -- AWS SDK --
if(UNIX)
    message("Unix - Adding ZLIB explicitly for AWS SDK")
    find_package(ZLIB REQUIRED)
endif()

# Path to built AWS SDK
set(AWS_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../aws_sdk/install")
list(APPEND CMAKE_PREFIX_PATH ${AWS_SDK_DIR})

find_package(AWSSDK REQUIRED COMPONENTS core rds secretsmanager sts)
list(APPEND EXTERNAL_LIBRARIES ${AWSSDK_LINK_LIBRARIES})

# -- ODBC --
if(APPLE)
    # Use UnixODBC instead of iODBC
    set(ODBC_INCLUDE_DIR /opt/homebrew/opt/unixodbc/include)
endif()

find_package(ODBC REQUIRED)
list(APPEND EXTERNAL_LIBRARIES ODBC::ODBC)

# Build Executable ------------------------------------------------------------------------------------------
if(WIN32)
    add_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN # Exclude rarely-used stuff from Windows headers
    )
endif()

if(UNICODE)
    add_compile_definitions(${COMPILE_FILE_NAME} PRIVATE
        UNICODE
        _UNICODE
    )
else()
    add_compile_definitions(${COMPILE_FILE_NAME} PRIVATE
        SQL_NOUNICODEMAP
    )
endif()

enable_testing()
add_executable(
    ${PROJECT_NAME}

    ${TEST_UTILITIES}
    ${TEST_SUITE}
    ${TEST_RUNNER}
)
include(GoogleTest)

# Link ------------------------------------------------------------------------------------------------------
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${WRAPPER_NAME}-static
    ${EXTERNAL_LIBRARIES}
)
