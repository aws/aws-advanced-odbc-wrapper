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

#ifndef SOCKET_H_
#define SOCKET_H_

#pragma once

#include "SocketSupport.h"
#include "Selector.h"
#include <string>

/*
* This class is used to wrap the network socket
* in order to have cross-platfrom code to send and receive
* data from incoming connections.
*/
class Socket
{
    public:
        Socket();
        
        Socket( SOCKET sfd);
        
        Socket(const Socket& s) = delete;
        
        Socket& operator=(const Socket& s) = delete;
        
        Socket(Socket&& s) noexcept;
        
        Socket& operator=(Socket&& s) noexcept ;
        
        ~Socket();
        
        /*
        * Close a socket file descriptor, return true on success
        * or false in case of error.
        */
        bool Close();
        
        /*
        * Get port where socket is listening.
        */
        int GetListenPort() const;

        /*
        * Listen for connections on a socket and return zero on success,
        * or -1 in case of error.
        */
        int Listen(int backlog) const;
        
        /*
        * Accept a connection on a socket and return StreamSocket object.
        */
        Socket Accept() const;
        
        /*
        * Forcibly bind to a port in use by another socket and return true on success,
        * or false in case of error.
        */
        bool SetReusable() const;
        
        /*
        * Set socket to non-blocking mode and return true on success
        * or false in case of error.
        */
        bool SetNonBlocking() const;
        
        /*
        * Return true if error is caused by non-blocking mode of socket
        * otherwise return false.
        */
        static bool IsNonBlockingError() ;
        
        /*
        * Prepare socket to handle incoming connections.
        */
        void PrepareListenSocket(const std::string& port);
        
        /*
        * Register socket in master file descriptor set using Selector class.
        */
        void Register(Selector& selector) const;
        
        /*
        * Clear socket in master file descriptor set using Selector class.
        */
        void Unregister(Selector& selector) const;
        
        /*
        * Receive a message from a socket and return the number of bytes received,
        * or -1 if an error occurred.
        */
        int Receive(char *buffer, int length, int flags) const;
        
        /*
        * Send a message on a socket and return the number of bytes sent,
        * or -1 if an error occured.
        */
        int Send(const char *buffer, int length, int flags) const;
        
        /*
        * Bind a name to a socket and return zero on success,
        * or -1 in case of error.
        */
        int Bind(const struct sockaddr *address, size_t address_len) const;
        
    private:

        const int CONNECTION_BACKLOG = 10;

        SOCKET socket_fd_;
};

#endif // SOCKET_H_
