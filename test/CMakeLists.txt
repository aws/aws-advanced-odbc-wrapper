cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project("poc-shell-test")
set(CMAKE_CXX_STANDARD 20)

# Sources
set(SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/simple_drv_conn.cpp
)

if(WIN32)
    add_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN # Exclude rarely-used stuff from Windows headers
        SQL_NOUNICODEMAP # For Testing
    )
endif()

# Build Exe
add_executable(${PROJECT_NAME}
    ${SRC}
)

# Link
# -- ODBC --
if(APPLE)
    # Use UnixODBC instead of iODBC
    set(ODBC_INCLUDE_DIR /opt/homebrew/opt/unixodbc/include)
endif()

find_package(ODBC REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ODBC::ODBC)
