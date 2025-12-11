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

#include "Selector.h"

void Selector::Register(SOCKET sfd)
{
    if (sfd == -1) {
        return;
    }

    FD_SET(sfd, &master_fds_);

#ifdef WIN32
    max_fd_ = max(max_fd_, sfd);
#else
    max_fd_ = std::max(max_fd_, sfd);
#endif
}

void Selector::Unregister(SOCKET sfd)
{
    FD_CLR(sfd, &master_fds_);
}

bool Selector::Select(struct timeval *tv)
{
    // As select will modify the file descriptor set we should keep temporary set to reflect the ready fd.
    fd_set read_fds = master_fds_;
    
    if (select(max_fd_ + 1, &read_fds, nullptr, nullptr, tv) > 0) {
        for (SOCKET sfd = 0; sfd <= max_fd_; sfd++) {
            if (FD_ISSET(sfd, &read_fds)) {
                return true;
            }
        }
    }
    
    return false;
}

Selector::Selector() : max_fd_(0)
{
    FD_ZERO(&master_fds_);
}
