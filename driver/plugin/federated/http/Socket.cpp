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

#include "AddrInformation.h"
#include "Socket.h"

#include <chrono>
#include <stdexcept>
#include <thread>

////////////////////////////////////////////////////////////////////////////////////////////////////
int Socket::Receive(char *buffer, int length, int flags) const
{
    int nbytes = 0;
    int filled = 0;
    auto start = std::chrono::system_clock
    ::now();

    const int receive_wait = 200;
    // Give a chance to fully receive packet in case if there is no data in
    // non-blocking socket.
    while ((std::chrono::system_clock::now() - start < std::chrono::seconds(1))) {
        nbytes = static_cast<int>(recv(socket_fd_, buffer + filled, length - filled - 1, 0));

        if (nbytes <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(receive_wait));
        } else {
            filled += nbytes;
        }
    }

    return filled == 0 ? nbytes : filled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int Socket::Send(const char *buffer, int length, int flags) const
 {
    int nbytes = 0;
    int sent = 0;

    while ((nbytes = static_cast<int>(send(socket_fd_, buffer + sent, length - sent, flags))) > 0) {
        sent += nbytes;
    }

    return sent == 0 ? nbytes : sent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Socket::PrepareListenSocket(const std::string& port)
{
    const AddrInformation addr_info(port);

    for (const auto& ptr : addr_info) {
        socket_fd_ = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        if (socket_fd_ == INVALID_SOCKET) {
            continue;
        }

        if (!SetReusable()) {
            Close();

            throw std::runtime_error("Unable to reuse the address.");
        }

        if (Bind(ptr->ai_addr, ptr->ai_addrlen) == 0) {
            break;
        }

        Close();
    }

    if (!SetNonBlocking()) {
        Close();

        throw std::runtime_error("Unable to set socket to non-blocking mode.");
    }

    if ((socket_fd_ == INVALID_SOCKET) || (Listen(CONNECTION_BACKLOG))) {
        Close();

        throw std::runtime_error("Can not start listening on port: " + port);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Socket::Close()
{
    if (socket_fd_ == -1) {
        return false;
    }

#if (defined(_WIN32) || defined(_WIN64))
    int res = closesocket(socket_fd_);
#else
    const int res = close(socket_fd_);
#endif

    socket_fd_ = INVALID_SOCKET;

    return res == 0;
}
