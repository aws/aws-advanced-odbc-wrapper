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

#include <cstring>
#include <stdexcept>

AddrInformation::AddrInformation( const std::string& port)
{
    addrinfo hints;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo("127.0.0.1", port.c_str(), &hints, &addrinfo_) != 0) {
        throw std::runtime_error("Unable to get address information.");
    }
}

AddrInformation::~AddrInformation()
{
    freeaddrinfo(addrinfo_);
}

AddrInformationIterator AddrInformation::begin()
{
    return {addrinfo_};
}

AddrInformationIterator AddrInformation::end()
{
    return {nullptr};
}

AddrInformationIterator AddrInformation::begin() const {
    return {addrinfo_};
}

AddrInformationIterator::AddrInformationIterator(addrinfo* addr) : addr_(addr)
{
    ; // Do nothing.
}

AddrInformationIterator::AddrInformationIterator(const AddrInformationIterator& itr)
    : addr_(itr.addr_)
{
    ; // Do nothing.
}

AddrInformationIterator::~AddrInformationIterator()
{
    ; // Do nothing.
}

AddrInformationIterator AddrInformationIterator::operator++(int)
{
    const AddrInformationIterator tmp(*this);

    if (addr_)
    {
        addr_ = addr_->ai_next;
    }

    return tmp;
}

AddrInformationIterator& AddrInformationIterator::operator++()
{
    if (addr_) {
        addr_ = addr_->ai_next;
    }

    return *this;
}

bool AddrInformationIterator::operator!=(const AddrInformationIterator& itr) const
{
    return addr_ != itr.addr_;
}

bool AddrInformationIterator::operator==(const AddrInformationIterator& itr) const
{
    return addr_ == itr.addr_;
}

addrinfo* AddrInformationIterator::operator->()
{
    return addr_;
}

const addrinfo* AddrInformationIterator::operator*() const
{
    return addr_;
}

addrinfo* AddrInformationIterator::operator*()
{
    return addr_;
}
