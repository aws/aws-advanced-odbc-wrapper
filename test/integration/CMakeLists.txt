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
project("integration-test")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configuration ----------------------------------------------------------------------------------------------
set(BUILD_UNICODE OFF CACHE BOOL "Toggle to build with Unicode tests, else Ansi")
set(BUILD_FAILOVER OFF CACHE BOOL "Build Failover Tests")
set(BUILD_LIMITLESS OFF CACHE BOOL "Build Limitless Tests")
set(USE_ASAN OFF CACHE BOOL "Toggle to use Address Sanitizer")

# Sources ----------------------------------------------------------------------------------------------------
set(TEST_UTILITIES
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/base_connection_test.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/connection_string_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/odbc_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/odbc_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/string_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/string_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/test_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/test_utils.cpp
)

set(TEST_SUITE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/connection_string_builder_test.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/iam_authentication_integration_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/secrets_manager_integration_test.cpp
)

if(BUILD_FAILOVER)
    list(APPEND TEST_SUITE
        ${CMAKE_CURRENT_SOURCE_DIR}/base_failover_integration_test.h
        ${CMAKE_CURRENT_SOURCE_DIR}/custom_endpoint_integration_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/failover_integration_test.cpp
    )
elseif(BUILD_LIMITLESS)
    list(APPEND TEST_SUITE
        ${CMAKE_CURRENT_SOURCE_DIR}/limitless_integration_test.cpp
    )
endif()

set(TEST_RUNNER
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/test_runner.cpp
)

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

# -- ODBC --
if(APPLE)
    # Use UnixODBC instead of iODBC
    set(ODBC_INCLUDE_DIR /opt/homebrew/opt/unixodbc/include)
endif()

find_package(ODBC REQUIRED)
list(APPEND EXTERNAL_LIBRARIES ODBC::ODBC)

# -- AWS SDK --
# Used for Failover Integration Tests
if(BUILD_FAILOVER)
    if(UNIX)
        message("Unix - Adding ZLIB explicitly for AWS SDK")
        find_package(ZLIB REQUIRED)
    endif()

    # Path to built AWS SDK
    set(AWS_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../aws_sdk/install")
    list(APPEND CMAKE_PREFIX_PATH ${AWS_SDK_DIR})

    find_package(AWSSDK REQUIRED COMPONENTS core rds sts)
    list(APPEND EXTERNAL_LIBRARIES ${AWSSDK_LINK_LIBRARIES})
endif()

if(USE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

    if(WIN32)
        add_compile_definitions(_DISABLE_STRING_ANNOTATION=1)
        add_compile_definitions(_DISABLE_VECTOR_ANNOTATION=1)
    endif()
endif()

# Build Executable ------------------------------------------------------------------------------------------
if(WIN32)
    add_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN # Exclude rarely-used stuff from Windows headers
    )
endif()

if(BUILD_UNICODE)
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
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${EXTERNAL_LIBRARIES})

# Copy External Dependencies --------------------------------------------------------------------------------
IF(BUILD_FAILOVER)
    if(MSVC)
        set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
    else()
        set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()
    message("Copying Libraries to: ${OUTPUT_DIR}")

    LIST(APPEND SERVICE_LIST rds secretsmanager sts)
    AWSSDK_CPY_DYN_LIBS(SERVICE_LIST "" ${OUTPUT_DIR})
ENDIF()

# ODBC Unix DSN ---------------------------------------------------------------------------------------------
SET(ODBC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../resources/)

# DSN Settings
SET(THREADING "0")

# To be set via flags
# -D TEST_DRIVER_PATH
# -D PG_DRIVER_PATH
# -D PG_SERVER
# -D PG_DATABASE
# -D MYSQL_DRIVER_PATH
# -D MYSQL_SERVER
# -D MYSQL_DATABASE

CONFIGURE_FILE(
    ${ODBC_PATH}/odbcinst.ini.in
    ${ODBC_PATH}/odbcinst.ini
    @ONLY
)
CONFIGURE_FILE(
    ${ODBC_PATH}/odbc.ini.in
    ${ODBC_PATH}/odbc.ini
    @ONLY
)
