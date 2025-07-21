cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project("poc-shell-driver")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configuration ----------------------------------------------------------------------------------------------
set(WITH_IODBC OFF CACHE BOOL "Build with iODBC" FORCE)

# Sources ----------------------------------------------------------------------------------------------------
set(INC
    # Plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/base_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/iam/iam_auth_plugin.h

    # Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/util/auth_provider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/aws_sdk_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_keys.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/odbc_dsn_helper.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/util/auth_provider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/aws_sdk_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/connection_string_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/odbc_dsn_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/rds_lib_loader.cpp

    # Core
    ${CMAKE_CURRENT_SOURCE_DIR}/odbcapi.cpp
)

set(EXP
    # Definition for exported functions
    ${CMAKE_CURRENT_SOURCE_DIR}/poc_shell_driver.def
)

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
    # TODO - Test Unix / iODBC
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
message("Linking External Libraries: [${EXTERNAL_LIBRARIES}]")
target_link_libraries(${PROJECT_NAME} PRIVATE ${EXTERNAL_LIBRARIES})
