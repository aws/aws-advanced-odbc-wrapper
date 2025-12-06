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

#pragma once

#include "SocketSupport.h"
#include <string>

/*
* This class is used to support range-based for loop and iterators
* in AddrInformation class.
*/
class AddrInformationIterator {
public:
    AddrInformationIterator(addrinfo *addr);

    AddrInformationIterator(const AddrInformationIterator& itr);

    ~AddrInformationIterator();

    AddrInformationIterator& operator++();

    AddrInformationIterator operator++(int);

    bool operator!=(const AddrInformationIterator& itr) const;

    bool operator==(const AddrInformationIterator& itr) const;

    addrinfo* operator->();

    const addrinfo* operator*() const;

    addrinfo* operator*();

private:
    addrinfo *addr_;

};

/*
* This class is used to perform network address and service
* translation via getaddrinfo call. Returns one or more addrinfo structures, each
* of which contains an Internet address.
*/
class AddrInformation
{
public:
    AddrInformation( const std::string& port);

    ~AddrInformation();

    AddrInformationIterator begin();

    AddrInformationIterator end();

    AddrInformationIterator begin() const;

    AddrInformationIterator end() const;

private:

    addrinfo *addrinfo_;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformation::~AddrInformation()
{
   freeaddrinfo(addrinfo_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformation::begin()
{
    return AddrInformationIterator(addrinfo_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformation::end()
{
    return AddrInformationIterator(nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformation::begin() const
{
    return AddrInformationIterator(addrinfo_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformation::end() const
{
    return AddrInformationIterator(nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator::AddrInformationIterator(addrinfo* addr) : addr_(addr)
{
    ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator::AddrInformationIterator(const AddrInformationIterator& itr)
    : addr_(itr.addr_)
{
    ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator::~AddrInformationIterator()
{
    ; // Do nothing.
}


////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator AddrInformationIterator::operator++(int)
{
    AddrInformationIterator tmp(*this);

    if (addr_)
    {
        addr_ = addr_->ai_next;
    }

    return tmp;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline AddrInformationIterator& AddrInformationIterator::operator++()
{
    if (addr_) {
        addr_ = addr_->ai_next;
    }

    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool AddrInformationIterator::operator!=(const AddrInformationIterator& itr) const
{
    return addr_ != itr.addr_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool AddrInformationIterator::operator==(const AddrInformationIterator& itr) const
{
    return addr_ == itr.addr_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline addrinfo* AddrInformationIterator::operator->()
{
    return addr_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline const addrinfo* AddrInformationIterator::operator*() const
{
    return addr_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline addrinfo* AddrInformationIterator::operator*()
{
    return addr_;
}
