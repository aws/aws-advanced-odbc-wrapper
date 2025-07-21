cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project("compatibility-test")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources ----------------------------------------------------------------------------------------------------
set(TEST_UTILITIES
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/connection_string_builder.h
)

set(TEST_SUITE
    ${CMAKE_CURRENT_SOURCE_DIR}/connection_test.cpp
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
add_compile_definitions(${PROJECT_NAME} PRIVATE
    SQL_NOUNICODEMAP # For Testing
)

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
SET(ODBC_PATH ${CMAKE_SOURCE_DIR}/../resources/)

SET(MY_VARIABLE "option_value" CACHE STRING "Some user-specified option")

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
