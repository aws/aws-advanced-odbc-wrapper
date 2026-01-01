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
list(APPEND CMAKE_BUILD_RPATH "@loader_path;$ORIGIN")
set(USE_ASAN OFF CACHE BOOL "Toggle to use Address Sanitizer")

# Sources ----------------------------------------------------------------------------------------------------
set(INC
    # Host Selectors
    ${CMAKE_CURRENT_SOURCE_DIR}/host_selector/highest_weight_host_selector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_selector/host_selector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_selector/random_host_selector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_selector/round_robin_host_selector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_selector/round_robin_property.h

    # Plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/base_plugin.h
    ## Failover
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/failover/cluster_topology_monitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/failover/cluster_topology_query_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/failover/failover_plugin.h
    ## Federated
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/adfs_auth_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/html_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/okta_auth_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/saml_util.h
    ## IAM
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/iam/iam_auth_plugin.h
    ## Secrets Manager
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/secrets_manager/secrets_manager_plugin.h
    ## Limitless
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/limitless/limitless_query_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/limitless/limitless_router_monitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/limitless/limitless_router_service.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/limitless/limitless_plugin.h

    # Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/util/attribute_validator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/auth_provider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/aws_sdk_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/cluster_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_keys.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/init_plugin_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/logger_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/odbc_dsn_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/odbc_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_lib_loader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_strings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/sliding_cache_map.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/sql_query_analyzer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/topology_service.h

    # Dialects
    ${CMAKE_CURRENT_SOURCE_DIR}/dialect/dialect_aurora_mysql.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dialect/dialect_aurora_postgres.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dialect/dialect.h

    # Core
    ${CMAKE_CURRENT_SOURCE_DIR}/driver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_rds_helper.h

    # Webserver
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/AddrInformation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/HtmlResponse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/Parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/Selector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/Socket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/SocketStream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/SocketSupport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/WEBServer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/WEBServer_utils.h
)

set(SRC
    # Host Selectors
    ${CMAKE_CURRENT_SOURCE_DIR}/host_selector/highest_weight_host_selector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_selector/random_host_selector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_selector/round_robin_host_selector.cpp

    # Plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/base_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/failover/cluster_topology_monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/failover/cluster_topology_query_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/failover/failover_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/adfs_auth_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/html_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/okta_auth_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/saml_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/iam/iam_auth_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/secrets_manager/secrets_manager_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/limitless/limitless_query_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/limitless/limitless_router_monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/limitless/limitless_router_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/limitless/limitless_plugin.cpp

    # Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/util/attribute_validator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/auth_provider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/aws_sdk_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/cluster_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/init_plugin_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/logger_wrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/odbc_dsn_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/odbc_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_lib_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/sliding_cache_map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/sql_query_analyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/topology_service.cpp

    # Core
    ${CMAKE_CURRENT_SOURCE_DIR}/host_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi_rds_helper.cpp

    # Webserver
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/AddrInformation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/Parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/Selector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/Socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/SocketStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/WEBServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/federated/http/WEBServer_utils.cpp
)

# GUI
if(WIN32)
    list(APPEND INC
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/resource.h
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/odbcsetup.rc
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/setup.h
    )
    list(APPEND SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/setup.cpp
    )
endif()

# Configure Build Defines ------------------------------------------------------------------------------------
add_compile_definitions(
    ODBCVER=0x0380
    WIN32_LEAN_AND_MEAN # Exclude rarely-used stuff from Windows headers
)

# Unicode
IF(BUILD_UNICODE OR NOT BUILD_ANSI)
    ## Configure
    SET(COMPILE_FILE_NAME_UNICODE ${PROJECT_NAME}-w)
    SET(COMPILE_FILE_NAME ${COMPILE_FILE_NAME_UNICODE})
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
    if(BUILD_UNIT_TEST) # Static Library for Unit Tests
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
    SET(COMPILE_FILE_NAME ${COMPILE_FILE_NAME_ANSI})
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
    if(BUILD_UNIT_TEST) # Static Library for Unit Tests
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

if(USE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

    if(WIN32)
        add_compile_definitions(_DISABLE_STRING_ANNOTATION=1)
        add_compile_definitions(_DISABLE_VECTOR_ANNOTATION=1)
    endif()
endif()

# Link External Library ------------------------------------------------------------------------------------------
LIST(APPEND SERVICE_LIST rds secretsmanager sts)
if(MSVC)
    set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
else()
    set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()
message("Copying Libraries to: ${OUTPUT_DIR}")

message("Linking External Libraries: [${EXTERNAL_LIBRARIES}]")
foreach(target ${COMPILE_FILE_NAME_UNICODE} ${COMPILE_FILE_NAME_ANSI})
    # Link External Dependencies ---------------------------------------------------------------------------------
    target_link_libraries(${target} PRIVATE ${EXTERNAL_LIBRARIES})

    # Static library to run unit tests against -------------------------------------------------------------------
    if(BUILD_UNIT_TEST)
        target_link_libraries(${target}-static PRIVATE ${EXTERNAL_LIBRARIES})
    endif()
endforeach()

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
    LIST(APPEND SERVICE_LIST rds secretsmanager sts)
    AWSSDK_CPY_DYN_LIBS(SERVICE_LIST "" ${OUTPUT_DIR})
else()
    set(CMAKE_BUILD_WITH_INSTALL_RPATH true)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/")
    foreach(dynlib IN LISTS AWS_DYN_LIBS)
        message("Working on: ${dynlib}")
        unset(LIB_PATH CACHE)
        find_library(LIB_PATH "aws-cpp-sdk-${dynlib}" "${AWSSDK_LIB_DIR}/${CONFIG}" NO_DEFAULT_PATH)
        if (NOT LIB_PATH)
            find_library(LIB_PATH "${dynlib}" "${AWSSDK_LIB_DIR}/${CONFIG}" NO_DEFAULT_PATH)
            if (NOT LIB_PATH)
                message(FATAL_ERROR "Couldn't find library aws-cpp-sdk-${dynlib} or ${dynlib}")
            endif()
        endif()
        file(COPY ${LIB_PATH} DESTINATION ${OUTPUT_DIR} FOLLOW_SYMLINK_CHAIN)
        message("Copied: ${LIB_PATH} to ${OUTPUT_DIR}")
    endforeach()
endif()
