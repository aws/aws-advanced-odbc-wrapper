cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project("poc-shell-driver")
set(CMAKE_CXX_STANDARD 20)

# Sources ----------------------------------------------------------------------------------------------------
set(INC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/auth_provider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/base_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/connection_string_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/connection_string_keys.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/driver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/iam_auth_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/odbcapi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rds_lib_loader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rds_strings.h
)

set(SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/base_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/connection_string_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/iam_auth_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/odbcapi.cpp
)

set(EXP
    ${CMAKE_CURRENT_SOURCE_DIR}/src/poc_shell_driver.def
)

if(WIN32)
    add_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN # Exclude rarely-used stuff from Windows headers
        SQL_NOUNICODEMAP # For Testing
        ODBCVER=0x0380
    )
endif()

# Build Library ----------------------------------------------------------------------------------------------
add_library(${PROJECT_NAME} SHARED
    ${INC}
    ${SRC}
    ${EXP}
)

# Link External Dependencies ---------------------------------------------------------------------------------
# -- AWS SDK --
# Paths to search when attempting to find the AWS SDK
# set(AWS_SDK_DIR "${CMAKE_SOURCE_DIR}/../aws_sdk/install")
# list(APPEND CMAKE_PREFIX_PATH ${AWS_SDK_DIR})
# string(REPLACE ";" "/aws-cpp-sdk-all;" SYSTEM_MODULE_PATH "${CMAKE_SYSTEM_PREFIX_PATH}/aws-cpp-sdk-all")
# list(APPEND CMAKE_PREFIX_PATH ${SYSTEM_MODULE_PATH})

# find_package(AWSSDK REQUIRED COMPONENTS core sts)
# # include_directories(${AWSSDK_INCLUDE_DIRS})
# target_link_libraries(${LIBRARY_NAME} ${AWSSDK_LINK_LIBRARIES})

# -- ODBC --
if(APPLE)
    # Use UnixODBC instead of iODBC
    set(ODBC_INCLUDE_DIR /opt/homebrew/opt/unixodbc/include)
    message("Using UnixODBC for Apple")
endif()

find_package(ODBC REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ODBC::ODBC)
