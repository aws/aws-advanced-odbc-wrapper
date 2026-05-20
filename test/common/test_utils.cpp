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
    #include <sys/socket.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
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
    if (value == nullptr || value[0] == '\0') {
        if (default_value[0] != '\0') {
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
    char ipstr[INET_ADDRSTRLEN] = {0};

    ClearMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname.c_str(), NULL, &hints, &servinfo)) != 0) {
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

bool TEST_UTILS::CanTcpConnect(const std::string& hostname, int port, int timeout_seconds) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif

    struct addrinfo hints;
    struct addrinfo* servinfo = nullptr;
    ClearMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &servinfo) != 0) {
        return false;
    }

    bool connected = false;
    for (struct addrinfo* p = servinfo; p != nullptr; p = p->ai_next) {
        int sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;

#ifdef _WIN32
        unsigned long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

        int rc = connect(sock, p->ai_addr, p->ai_addrlen);
        if (rc == 0) {
            connected = true;
        } else {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                fd_set writefds;
                FD_ZERO(&writefds);
                FD_SET(sock, &writefds);
                struct timeval tv;
                tv.tv_sec = timeout_seconds;
                tv.tv_usec = 0;
                if (select(sock + 1, nullptr, &writefds, nullptr, &tv) > 0) {
                    int err = 0;
                    int len = sizeof(err);
                    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
                    connected = (err == 0);
                }
            }
#else
            if (errno == EINPROGRESS) {
                struct pollfd pfd;
                pfd.fd = sock;
                pfd.events = POLLOUT;
                if (poll(&pfd, 1, timeout_seconds * 1000) > 0) {
                    int err = 0;
                    socklen_t len = sizeof(err);
                    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
                    connected = (err == 0);
                }
            }
#endif
        }

#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif

        if (connected) break;
    }

    freeaddrinfo(servinfo);
    return connected;
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
