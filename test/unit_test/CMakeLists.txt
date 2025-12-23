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
set(USE_ASAN OFF CACHE BOOL "Toggle to use Address Sanitizer")

# Sources ----------------------------------------------------------------------------------------------------
set(TEST_UTILITIES
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/connection_string_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/auth_mock_objects.h
    ${CMAKE_CURRENT_SOURCE_DIR}/failover_mock_objects.h
    ${CMAKE_CURRENT_SOURCE_DIR}/limitless_mock_objects.h
)

set(TEST_SUITE
    ${CMAKE_CURRENT_SOURCE_DIR}/adfs_auth_plugin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adfs_saml_util_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/attribute_validator_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/auth_provider_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connection_string_helper_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/failover_plugin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/highest_weight_host_selector_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iam_auth_plugin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/okta_auth_plugin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/okta_saml_util_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/random_host_selector_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rds_utils_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/round_robin_host_selector_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/secrets_manager_plugin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sliding_cache_map_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sql_query_analyzer_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/html_util_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/init_plugin_helper_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/limitless_plugin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/limitless_router_service_test.cpp
)

set(TEST_RUNNER
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/test_runner.cpp
)

# Configure Build Defines ------------------------------------------------------------------------------------
if(BUILD_UNICODE)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${WRAPPER_NAME}-static
    ${EXTERNAL_LIBRARIES}
)

gtest_discover_tests(${PROJECT_NAME})

# Copy ICU for Windows Builds ------------------------------------------------------------------------------------
if(WIN32)
    string(REGEX MATCH "^[0-9]+" ICU_MAJOR ${ICU_VERSION})
    set(THIRD_PARTY_DLL
        ${ICU_ROOT}/bin64/icuuc${ICU_MAJOR}.dll
        ${ICU_ROOT}/bin64/icudt${ICU_MAJOR}.dll
    )
    foreach(dll_path IN LISTS THIRD_PARTY_DLL)
        message("Copying File ${dll_path}")
        file(COPY ${dll_path} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
    endforeach()
endif()

# Copy External Dependencies -------------------------------------------------------------------------------------
if(MSVC)
    set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
else()
    set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()
message("Copying Libraries to: ${OUTPUT_DIR}")

LIST(APPEND SERVICE_LIST rds secretsmanager sts)
AWSSDK_CPY_DYN_LIBS(SERVICE_LIST "" ${OUTPUT_DIR})
