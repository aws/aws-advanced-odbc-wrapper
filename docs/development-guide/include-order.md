# Include Order Standard

This document defines the `#include` standard for the AWS Advanced ODBC Wrapper. It is enforced by
`.clang-format` (`IncludeCategories`) and `.clang-tidy` (`llvm-include-order`).

## Table of Contents

- [The Rules](#the-rules)
- [Windows and ODBC Headers](#windows-and-odbc-headers)
- [Header Guards](#header-guards)
- [Forward Declarations](#forward-declarations)
- [Running the Formatter](#running-the-formatter)

## The Rules

1. Includes its own header first. `foo.cpp` starts with `#include "foo.h"`, before anything else.
2. Headers are grouped in this order, separated by a blank line. `clang-format` sorts within each group alphabetically and will move includes between groups as needed:

   | Priority | Group                                                                                      |
   |---------:|--------------------------------------------------------------------------------------------|
   |        0 | The matching header for a `.cpp` file                                                      |
   |        1 | `driver/util/windows_headers.h` (see below)                                                |
   |        2 | ODBC headers: `<sql.h>`, `<sqlext.h>`, `<sqltypes.h>`, `<odbcinst.h>`, `<sqlucode.h>` |
   |        3 | Other platform/system C headers: `<tchar.h>`, `<dlfcn.h>`, `<unistd.h>`, ...               |
   |        4 | C++ standard library: `<string>`, `<memory>`, `<vector>`, ...                              |
   |        5 | Third party: `<aws/...>`, `<ng-log/...>`, `"unicode/..."`, `<gtest/...>`, `<gmock/...>`    |
   |        6 | Project-local headers: `"driver.h"`, `"../util/rds_utils.h"`, ...                          |

3. Files must include what it uses, and does not rely on a transitive include.
4. If possible prefer a forward declaration to an include, see [Forward Declarations](#forward-declarations).

## Windows and ODBC Headers

ODBC headers such as `#include <sql.h>` depend on Window headers such as `<windows.h>`. However, never include `<windows.h>`, `<winsock2.h>`, or `<ws2tcpip.h>` directly as this will not only break on non-Windows systems, it may also cause conflicts with other dependencies such as the AWS SDK. Instead, include the following header:

```cpp
#include "../util/windows_headers.h"   // adjust the relative path
```

`driver/util/windows_headers.h` is empty on non-Windows platforms, so no `#ifdef` is needed. On Windows it:

- defines `WIN32_LEAN_AND_MEAN` and `NOMINMAX` before the first Windows header,
- includes `<winsock2.h>`, `<ws2tcpip.h>`, and `<windows.h>` in that order,
- `#undef`s the `GetObject`, `OUT`, `IN`, and `OPTIONAL` macros to avoid conflicts with AWS SDK, 
- and links `Ws2_32.lib` on MSVC.

## Header Guards

Use `#ifndef` / `#define` / `#endif` guards, named after the file in `UPPER_SNAKE_CASE` with a trailing underscore, and close with a comment:

```cpp
#ifndef RDS_UTILS_H_
#define RDS_UTILS_H_
// ...
#endif // RDS_UTILS_H_
```

## Forward Declarations

For headers, if a type is only used as a pointer, reference, or return type, forward-declare it instead of including its header. This helps to reduce build times as well as reduce circular dependencies. The corresponding `.cpp` includes the real headers to be able to access the member.

## Running the Formatter

To sort the includes in the files you touched:

```bash
clang-format -i <files>
```
