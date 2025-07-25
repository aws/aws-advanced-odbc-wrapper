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
project("aws-odbc-wrapper")

# Configuration ----------------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(WITH_IODBC OFF CACHE BOOL "Build with iODBC" FORCE)

# External Tools ---------------------------------------------------------------------------------------------
include(FetchContent)

# Sources ----------------------------------------------------------------------------------------------------
set(INC
    # Plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/base_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/adfs_auth_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/html_util.h
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
if(UNICODE)
    SET(COMPILE_FILE_NAME ${PROJECT_NAME}-w)
    message("Building for Unicode as: ${COMPILE_FILE_NAME}")
    add_compile_definitions(${COMPILE_FILE_NAME} PRIVATE
        UNICODE
        _UNICODE
    )
else()
    SET(COMPILE_FILE_NAME ${PROJECT_NAME}-a)
    message("Building for Ansi as: ${COMPILE_FILE_NAME}")
    add_compile_definitions(${COMPILE_FILE_NAME} PRIVATE
        SQL_NOUNICODEMAP
    )
endif()

add_compile_definitions(${COMPILE_FILE_NAME} PRIVATE
    ODBCVER=0x0380
)

if(WIN32)
    add_compile_definitions(${COMPILE_FILE_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN # Exclude rarely-used stuff from Windows headers
    )
endif()

# Configure Export Defines ----------------------------------------------------------------------------------
if(UNICODE)
    list(APPEND SRC ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_unicode.cpp)
    set(CHAR_ENCODING "W")
else(UNICODE)
    list(APPEND SRC ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_ansi.cpp)
    set(CHAR_ENCODING "")
endif(UNICODE)

if(WIN32)
    configure_file(
        ${CMAKE_SOURCE_DIR}/driver.def.cmake
        ${CMAKE_SOURCE_DIR}/driver${CONNECTOR_DRIVER_TYPE_SHORT}.def
        @ONLY
    )
    set(EXP
        # Definition for exported functions
        ${CMAKE_SOURCE_DIR}/driver${CONNECTOR_DRIVER_TYPE_SHORT}.def
    )
endif()

# Fetch External Libraries  ----------------------------------------------------------------------------------
# -- AWS SDK --
if(UNIX)
    message("Unix - Adding ZLIB explicitly for AWS SDK")
    find_package(ZLIB REQUIRED)
endif()

# Path to built AWS SDK
set(AWS_SDK_DIR "${CMAKE_SOURCE_DIR}/../aws_sdk/install")
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

# Build Library ----------------------------------------------------------------------------------------------
add_library(${COMPILE_FILE_NAME} SHARED
    ${INC}
    ${SRC}
    ${EXP}
)

# Link External Dependencies ---------------------------------------------------------------------------------
message("Linking External Libraries: [${EXTERNAL_LIBRARIES}]")
target_link_libraries(${COMPILE_FILE_NAME} PRIVATE ${EXTERNAL_LIBRARIES})
