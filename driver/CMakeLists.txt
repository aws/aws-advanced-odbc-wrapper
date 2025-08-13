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

# Configuration ----------------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Sources ----------------------------------------------------------------------------------------------------
set(INC
    # Plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/base_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/adfs_auth_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/html_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/okta_auth_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/saml_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/iam/iam_auth_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/secrets_manager/secrets_manager_plugin.h

    # Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/util/auth_provider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/aws_sdk_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_keys.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/logger_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/odbc_dsn_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_lib_loader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_strings.h

    # Core
    ${CMAKE_CURRENT_SOURCE_DIR}/driver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_rds_helper.h
)

set(SRC
    # GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/setup.cpp

    # Plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/base_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/adfs_auth_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/html_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/okta_auth_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/saml_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/iam/iam_auth_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/secrets_manager/secrets_manager_plugin.cpp

    # Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/util/auth_provider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/aws_sdk_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/logger_wrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/odbc_dsn_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_lib_loader.cpp

    # Core
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_rds_helper.cpp
)

# Configure Build Defines ------------------------------------------------------------------------------------
add_compile_definitions(
    ODBCVER=0x0380
    WIN32_LEAN_AND_MEAN # Exclude rarely-used stuff from Windows headers
)

# Unicode
IF(BUILD_UNICODE OR NOT BUILD_ANSI)
    ## Configure
    SET(COMPILE_FILE_NAME_UNICODE ${PROJECT_NAME}-w)
    message("Building for Unicode as: ${COMPILE_FILE_NAME_UNICODE}")
    list(APPEND COMPILE_DEFINITIONS_UNICODE
        UNICODE
        _UNICODE
    )
    set(CHAR_ENCODING "W")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/driver.def.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/driver${CHAR_ENCODING}.def
        @ONLY
    )

    ## Build
    add_library(${COMPILE_FILE_NAME_UNICODE} SHARED
        ${INC}
        ${SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_unicode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/driver${CHAR_ENCODING}.def
    )
    target_compile_definitions(${COMPILE_FILE_NAME_UNICODE} PRIVATE
        ${COMPILE_DEFINITIONS_UNICODE}
    )
    if(UNIT_TEST) # Static Library for Unit Tests
        add_library(${COMPILE_FILE_NAME_UNICODE}-static STATIC
            ${INC}
            ${SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_unicode.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/driver${CHAR_ENCODING}.def
        )
        target_compile_definitions(${COMPILE_FILE_NAME_UNICODE}-static PRIVATE
            ${COMPILE_DEFINITIONS_UNICODE}
        )
    endif()
endif()

# Ansi
IF(BUILD_ANSI OR NOT BUILD_UNICODE)
    ## Configure
    SET(COMPILE_FILE_NAME_ANSI ${PROJECT_NAME}-a)
    message("Building for Ansi as: ${COMPILE_FILE_NAME_ANSI}")
    list(APPEND COMPILE_DEFINITIONS_ANSI
        SQL_NOUNICODEMAP
    )
    set(CHAR_ENCODING "")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/driver.def.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/driver${CHAR_ENCODING}.def
        @ONLY
    )

    ## Build
    add_library(${COMPILE_FILE_NAME_ANSI} SHARED
        ${INC}
        ${SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_ansi.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/driver${CHAR_ENCODING}.def
    )
    target_compile_definitions(${COMPILE_FILE_NAME_ANSI} PRIVATE
        ${COMPILE_DEFINITIONS_ANSI}
    )
    if(UNIT_TEST) # Static Library for Unit Tests
        add_library(${COMPILE_FILE_NAME_ANSI}-static STATIC
            ${INC}
            ${SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_ansi.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/driver.def
        )
        target_compile_definitions(${COMPILE_FILE_NAME_ANSI} PRIVATE
            ${COMPILE_DEFINITIONS_ANSI}
        )
    endif()
endif()

# Link External Library ------------------------------------------------------------------------------------------
message("Linking External Libraries: [${EXTERNAL_LIBRARIES}]")
foreach(target ${COMPILE_FILE_NAME_UNICODE} ${COMPILE_FILE_NAME_ANSI})
    # Link External Dependencies ---------------------------------------------------------------------------------
    target_link_libraries(${target} PRIVATE ${EXTERNAL_LIBRARIES})

    # Static library to run unit tests against -------------------------------------------------------------------
    if(UNIT_TEST)
        target_link_libraries(${target}-static PRIVATE ${EXTERNAL_LIBRARIES})
    endif()
endforeach()
