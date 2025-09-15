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

# Sources ----------------------------------------------------------------------------------------------------
set(TEST_UTILITIES
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/connection_string_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/string_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/integration_test_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/integration_test_utils.cpp
)

set(TEST_SUITE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/connection_string_builder_test.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/iam_authentication_integration_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/secrets_manager_integration_test.cpp
)

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

# ODBC Unix DSN ---------------------------------------------------------------------------------------------
SET(ODBC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../resources/)

# Common Settings
SET(THREADING "0")
SET(TEST_SERVER ${TEST_SERVER},${TEST_PORT})
SET(TEST_DATABASE ${TEST_DATABASE})

# Test Driver
SET(TEST_DSN "wrapper-dsn")
SET(TEST_DRIVER_NAME "aws-odbc-wrapper")
SET(TEST_DRIVER_PATH ${TEST_DRIVER_PATH})

# Base PG Driver
SET(BASE_PG_DSN "pg-dsn")
SET(BASE_PG_DRIVER_NAME "community-pg")
SET(BASE_PG_DRIVER_PATH ${BASE_PG_DRIVER_PATH})

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
