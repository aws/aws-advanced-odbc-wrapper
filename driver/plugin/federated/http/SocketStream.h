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

#ifndef SOCKET_STREAM_H_
#define SOCKET_STREAM_H_

#pragma once

#include "Socket.h"

#include <sstream>

/*
* This class is used to wrap socket by std::stream.
*/
class SocketStream : public std::streambuf
{
    public:
        /*
        * Construct object and set the value for the pointers that define
        * the boundaries of the buffered portion of the controlled INPUT sequence.
        */
        SocketStream(Socket& s);
        
        /*
        * Call sync inside the destructor to synchronize the contents in the stream buffer
        * with those of the associated character sequence.
        */
        virtual ~SocketStream();
        
    protected:
        /*
        * Virtual function called by other member functions to get the current character
        * in the controlled input sequence without changing the current position.
        * It is called by public member functions such as sgetc to request
        * a new character when there are no read positions available at the get pointer (gptr).
        */
        virtual int underflow();

        static const int SIZE = 2048;
        static const int MAX_SIZE = 16384;

        int received_size_;
        char input_buffer_[SIZE];
        Socket& socket_;
};

#endif // SOCKET_STREAM_H_
