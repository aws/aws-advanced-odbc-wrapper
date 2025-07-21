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
