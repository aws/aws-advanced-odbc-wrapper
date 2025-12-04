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

#include "SocketStream.h"

SocketStream::SocketStream(Socket& s) : received_size_(0), socket_(s)
{
    setg(input_buffer_, input_buffer_, input_buffer_);
}

SocketStream::~SocketStream()
{
    sync();
}

int SocketStream::underflow()
{
    if (gptr() < egptr()) {
        return traits_type::to_int_type(*gptr());
    }
    
    const int received_bytes = socket_.Receive(input_buffer_, SIZE - 1, 0);
    
    /*
    * Return EOF in the following cases:
    * If the received bytes less than zero (error situation);
    * If the received bytes equal to zero (socket peer has performed an orderly shutdown);
    * If the length of received packets more than MAX_SIZE.
    */
    if ((received_bytes <= 0) || (received_size_ + received_bytes > MAX_SIZE)) {
        return traits_type::eof();
    }
    
    received_size_ += received_bytes;
    
    setg(input_buffer_, input_buffer_, input_buffer_ + received_bytes);
    
    return traits_type::to_int_type(*gptr());
}
