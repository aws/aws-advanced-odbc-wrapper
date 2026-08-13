// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef WINDOWS_HEADERS_H_
#define WINDOWS_HEADERS_H_

// Include this header first for any resources which
// require Windows Headers, e.g. ODBC's "sql.h"

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    // Ordered for WIN32_LEAN_AND_MEAN
    // clang-format off
    #include <winsock2.h> // NOLINT(llvm-include-order)
    #include <ws2tcpip.h>
    #include <windows.h>
    // clang-format on

    // Undef Window macros which conflict with AWS SDK
    #ifdef GetObject
        #undef GetObject
    #endif
    #ifdef OUT
        #undef OUT
    #endif
    #ifdef IN
        #undef IN
    #endif
    #ifdef OPTIONAL
        #undef OPTIONAL
    #endif

    // Resolves symbols for __imp_WSAStartup, __imp_socket, __imp_WSACleanup
    #ifdef _MSC_VER
        #pragma comment(lib, "Ws2_32.lib")
    #endif

#endif // defined(_WIN32) || defined(_WIN64)

#endif // WINDOWS_HEADERS_H_
