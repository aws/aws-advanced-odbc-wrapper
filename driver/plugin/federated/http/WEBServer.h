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

#ifndef WEBSERVER_H_
#define WEBSERVER_H_

#pragma once

#include "Parser.h"
#include "Selector.h"
#include "Socket.h"

#include <atomic>
#include <thread>

/*
* This class is used to launch the HTTP WEB server in separate thread
* to wait for the redirect response from the /oauth2/authorize and
* extract the authorization code from it.
*/
class WEBServer
{
    public:
        WEBServer(
            std::string& state,
            std::string& port,
            std::string& timeout);
        
        ~WEBServer() = default;
        
        /*
        * Launch the HTTP WEB server in separate thread.
        */
        void LaunchServer();
        
        /*
        * Wait until HTTP WEB server is finished.
        */
        void Join();
        
        /*
        * Extract the authorization code from response.
        */
        std::string GetCode() const;
        
        /*
        * Get port where server is listening.
        */
        int GetListenPort() const;
        
        /*
        * Extract the SAML Assertion from response.
        */
        std::string GetSamlResponse() const;
        
        /*
        * If server is listening for connections return true, otherwise return false.
        */
        bool IsListening() const;
        
        /*
        * If timeout happened return true, otherwise return false.
        */
        bool IsTimeout() const;

        /*
        * Cancel listen loop prematurely without waiting for the timeout.
        */
        void Cancel();

    private:
        /*
        * Main HTTP WEB server function that perform initialization and
        * listen for incoming connections for specified time by user.
        */
        void ListenerThread();
                
        /*
        * If incoming connection is available call HandleConnection.
        */
        void Listen();
                
        /*
        * Launch parser if incoming connection is acceptable.
        */
        void HandleConnection();
                
        /*
        * Perform socket preparation to launch the HTTP WEB server.
        */
        bool WEBServerInit();

        std::string state_;
        std::string port_;
        int timeout_;
        std::string code_;
        std::thread thread_;
        Selector selector_;
        Parser parser_;
        Socket listen_socket_;
        int listen_port_;
        int connections_counter_;
        std::atomic<bool> listening_;
        bool cancel_ = false;
};

#endif // WEBSERVER_H_
