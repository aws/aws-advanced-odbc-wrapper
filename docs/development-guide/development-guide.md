# Build and Test AWS Advanced ODBC Wrapper

## Table of Contents
- [Build and Test AWS Advanced ODBC Wrapper](#build-and-test-aws-odbc-wrapper)
  - [Table of Contents](#table-of-contents)
  - [Building the Wrapper](#building-the-wrapper)
    - [Wrapper CMake Parameters](#wrapper-cmake-parameters)
    - [Windows](#windows)
    - [MacOS](#macos)
    - [Linux](#linux)
  - [Build and Run Tests](#build-and-run-tests)

## Building the Wrapper

### Wrapper CMake Parameters
| Key | Possible Values | Default Value | Explanation |
|-----------------|:-------------:|:-----:|-|
| BUILD_ANSI      | `ON` / `OFF`  | `OFF` | Toggle to `ON` to build the **ANSI version** of the wrapper. By default, if both UNICODE and ANSI are `OFF`, both will be built. |
| BUILD_UNICODE   | `ON` / `OFF`  | `OFF` | Toggle to `ON` to build the **UNICODE version** of the wrapper. By default, if both UNICODE and ANSI are `OFF`, both will be built. |
| BUILD_UNIT_TEST | `ON` / `OFF`  | `OFF` | Toggle to `ON` to build the **Unit Tests**. |

### Windows

#### Prerequisites
1. Install [CMake](https://cmake.org/download/) and add to environment Path
2. Install MSBuild, part of [Microsoft Visual Studio](https://visualstudio.microsoft.com/downloads/) C++ Desktop Development, and add to environment Path
3. Build AWS SDK for C++
```
./scripts/compile_aws_sdk_win.ps1 <Release/Debug>
```

#### Build Wrapper
```
# In Repository Root
cmake -S . -B build -DBUILD_UNICODE=<ON/OFF> -DBUILD_ANSI=<ON/OFF> -DBUILD_UNIT_TEST=<ON/OFF>
cmake --build build --config <Release/Debug>
```

### MacOS

#### Prerequisites
1. Install [Homebrew](https://brew.sh) and get latest updates
```
brew update && brew update && brew cleanup
```
2. Install build dependencies
```
brew install cmake curl openssl unixodbc zlib
```
3. Build AWS SDK for C++
```
./scripts/compile_aws_sdk_unix.sh <Release/Debug>
```

#### Build Wrapper
```
# In Repository Root
cmake -S driver -B build -DBUILD_UNICODE=<ON/OFF> -DBUILD_ANSI=<ON/OFF> -DBUILD_UNIT_TEST=<ON/OFF> -DCMAKE_BUILD_TYPE=<Release/Debug>
cmake --build build
```

### Linux

#### Prerequisites
1. Install build dependencies
```
sudo apt update
sudo apt-get install cmake libcurl4-openssl-dev libssl-dev odbcinst unixodbc-dev uuid-dev zlib1g-dev
```
2. Build AWS SDK for C++
```
./scripts/compile_aws_sdk_unix.sh <Release/Debug>
```

#### Build Wrapper
```
# In Repository Root
cmake -S driver -B build -DBUILD_UNICODE=<ON/OFF> -DBUILD_ANSI=<ON/OFF> -DBUILD_UNIT_TEST=<ON/OFF> -DCMAKE_BUILD_TYPE=<Release/Debug>
cmake --build build
```

## Build and Run Tests
There are multiple types of test, each type will be in its own folder under `test`.

The unit tests is built along side the driver by passing in the CMake flag `-DUNITTEST` and can be ran manually from the build folder
e.g.
Note: Windows will build binaries into a subfolder of the build config while Unix does not.
```
./build_folder/test/unit_test/<Release/Debug/nil>/unit-test
```
The following will go over how to build compatibility tests, in particular, how to test against PostgreSQL.

### Building
```
cmake -S test/compatibility -B test_compatibility \
    -DUNICODE=<ON/OFF> \
    -DCMAKE_BUILD_TYPE=<Release/Debug> \
    -DTEST_SERVER="<Test Database Host>" \
    -DTEST_PORT="<Test Database Port>" \
    -DTEST_DATABASE="<Test Database>" \
    -DTEST_DRIVER_PATH="<Path to AWS Advanced ODBC Wrapper>" \
    -DBASE_PG_DRIVER_PATH="<Path to PostgreSQL Driver>"

cmake --build test_compatibility
```

### Running the Tests
The tests will use environment variables to construct connection strings.
Each test will vary on what is required to run the test.
For compatibility tests, the follow are needed:
```
TEST_SERVER: Host of Test Database Server
TEST_PORT: Port of Test Database Server
TEST_DSN: DSN Name created for the Wrapper
TEST_DATABASE: Test Database to use
TEST_USERNAME: Username for the database
TEST_PASSWORD: Password for the database
TEST_BASE_DRIVER: A full path to the driver to wrap against
TEST_BASE_DSN: DSN Name created for driver to be wrapped against
```
and to run after setting environment variables.

Note: Windows will build binaries into a subfolder of the build config while Unix does not.
```
./test_compatibility/<Release/Debug/nil>/compatibility-test
```
