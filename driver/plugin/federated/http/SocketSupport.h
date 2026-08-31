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

#ifndef SOCKET_SUPPORT_H_
#define SOCKET_SUPPORT_H_

#if (defined(_WIN32) || defined(_WIN64))

#include "../../../util/windows_headers.h"

#else /* Linux or MAC */

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef int SOCKET;
#define INVALID_SOCKET -1

#endif

#endif // SOCKET_SUPPORT_H_
