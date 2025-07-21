cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project("poc-shell-driver")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources ----------------------------------------------------------------------------------------------------
set(INC
    # Plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/base_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/iam/iam_auth_plugin.h

    # Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/util/auth_provider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_keys.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_lib_loader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_strings.h

    # Core
    ${CMAKE_CURRENT_SOURCE_DIR}/driver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi.h
)

set(SRC
    # Plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/base_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/iam/iam_auth_plugin.cpp

    # Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_helper.cpp

    # Core
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi.cpp
)

set(EXP
    # Definition for exported functions
    ${CMAKE_CURRENT_SOURCE_DIR}/poc_shell_driver.def
)

# Fetch External Libraries  ----------------------------------------------------------------------------------
# -- AWS SDK --
# Paths to search when attempting to find the AWS SDK
# set(AWS_SDK_DIR "${CMAKE_SOURCE_DIR}/../aws_sdk/install")
# list(APPEND CMAKE_PREFIX_PATH ${AWS_SDK_DIR})
# string(REPLACE ";" "/aws-cpp-sdk-all;" SYSTEM_MODULE_PATH "${CMAKE_SYSTEM_PREFIX_PATH}/aws-cpp-sdk-all")
# list(APPEND CMAKE_PREFIX_PATH ${SYSTEM_MODULE_PATH})

# find_package(AWSSDK REQUIRED COMPONENTS core sts)
# # include_directories(${AWSSDK_INCLUDE_DIRS})
# target_link_libraries(${LIBRARY_NAME} ${AWSSDK_LINK_LIBRARIES})
# list(APPEND EXTERNAL_LIBRARIES ${AWSSDK_LINK_LIBRARIES})

# -- ODBC --
if(APPLE)
    # Use UnixODBC instead of iODBC
    set(ODBC_INCLUDE_DIR /opt/homebrew/opt/unixodbc/include)
    message("Using UnixODBC for Apple")
endif()

find_package(ODBC REQUIRED)
list(APPEND EXTERNAL_LIBRARIES ODBC::ODBC)

# Build Library ----------------------------------------------------------------------------------------------
if(WIN32)
    add_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN # Exclude rarely-used stuff from Windows headers
    )
endif()

add_compile_definitions(${PROJECT_NAME} PRIVATE
    SQL_NOUNICODEMAP # For Testing
    ODBCVER=0x0380
)

add_library(${PROJECT_NAME} SHARED
    ${INC}
    ${SRC}
    ${EXP}
)

# Link External Dependencies ---------------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE ${EXTERNAL_LIBRARIES})
