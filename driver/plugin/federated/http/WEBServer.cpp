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

#include "../../../util/logger_wrapper.h"
#include "HtmlResponse.h"
#include "SocketStream.h"
#include "WEBServer.h"

#include <array>
#include <chrono>
#include <exception>
#include <format>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <string>

////////////////////////////////////////////////////////////////////////////////////////////////////
bool WEBServer::WEBServerInit()
{
    // Prepare the environment for get the socket description.
    try {
        listen_socket_.PrepareListenSocket(port_);
        listen_socket_.Register(selector_);
        listen_port_ = listen_socket_.GetListenPort();
    } catch (std::exception& e) {
        DLOG(INFO) << "Exception: " << e.what();

        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WEBServer::HandleConnection()
{
    /* Trying to accept the pending incoming connection. */
    Socket ssck(listen_socket_.Accept());
    
    ++connections_counter_;
    
    if (ssck.SetNonBlocking()) {
        SocketStream socket_buffer(ssck);
        std::istream socket_stream(&socket_buffer);

        const STATUS status = parser_.Parse(socket_stream);

        if (status == STATUS::SUCCEED) {
            ssck.Send(ValidResponse.c_str(), static_cast<int>(ValidResponse.size()), 0);
        } else if (status == STATUS::FAILED) {
            ssck.Send(InvalidResponse.c_str(), static_cast<int>(InvalidResponse.size()), 0);
        } else if (status == STATUS::GET_SUCCESS) {
            const std::string response = std::vformat(GetResponse, std::make_format_args(listen_port_));
            ssck.Send(response.c_str(), static_cast<int>(response.size()), 0);
        } else {
            /* Nothing is received from socket. Continue to listen. */
            return;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WEBServer::Listen()
{
    // Set timeout for non-blocking socket to 1 sec to pass it to Select.
    struct timeval tv = { .tv_sec=1, .tv_usec=0 };
    
    if (selector_.Select(&tv)) {
        HandleConnection();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WEBServer::ListenerThread()
{
    if (!WEBServerInit()) {
        DLOG(INFO) << "WEBServerInit Failed";
        return;
    }

    auto start = std::chrono::system_clock::now();

    listening_.store(true);

    while ((std::chrono::system_clock::now() - start < std::chrono::seconds(timeout_)) && !parser_.IsFinished()) {
        Listen();
    }

    code_ = parser_.RetrieveAuthCode(state_);

    listen_socket_.Close();
}
