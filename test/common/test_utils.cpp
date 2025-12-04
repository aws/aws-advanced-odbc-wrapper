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

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdio.h>
    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    /* Below adds memset_s if available */
    #ifdef __STDC_LIB_EXT1__
        #define __STDC_WANT_LIB_EXT1__ 1
        #include <string.h> // memset_s
    #endif  /* __STDC_WANT_LIB_EXT1__ */
#endif

#include <cmath> // std::round

#include <gtest/gtest.h>

#include "../common/string_helper.h"

#include "test_utils.h"

std::string TEST_UTILS::GetEnvVar(const char* key, const char* default_value) {
    char* value = std::getenv(key);
    if (value == nullptr || value == "") {
        if (default_value == "") {
            std::cout << "Unable to get environment variable for: " << key << ", using default: " << default_value << std::endl;
        }

        return default_value;
    }

    return value;
}

int TEST_UTILS::StrToInt(std::string str) {
    const long int x = strtol(str.c_str(), nullptr, 0);
    assert(x <= INT_MAX);
    assert(x >= INT_MIN);
    return static_cast<int>(x);
}

std::string TEST_UTILS::HostToIp(std::string hostname) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
    int status;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    char ipstr[INET_ADDRSTRLEN];

    ClearMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname.c_str(), NULL, &hints, &servinfo)) != 0) {
        ADD_FAILURE() << "The IP address of host " << hostname << " could not be determined."
            << "getaddrinfo error:" << gai_strerror(status);
        return {};
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        void* addr;

        struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
        addr = &(ipv4->sin_addr);
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
    }

    freeaddrinfo(servinfo);
    return std::string(ipstr);
}

void TEST_UTILS::ClearMemory(void* dest, size_t count) {
    #ifdef _WIN32
        SecureZeroMemory(dest, count);
    #else
        #ifdef __STDC_LIB_EXT1__
            memset_s(dest, count, '\0', count);
        #else
            memset(dest, '\0', count);
        #endif /* __STDC__LIB_EXT1__*/
    #endif /* _WIN32 */

    return;
}
