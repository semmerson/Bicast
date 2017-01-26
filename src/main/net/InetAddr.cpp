/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddr.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an Internet address.
 */

#include "InetAddr.h"
#include "InetAddrImpl.h"
#include "Ipv4Addr.h"
#include "Ipv6Addr.h"

namespace hycast {

InetAddr::InetAddr(InetAddrImpl* impl)
        : pImpl{impl}
{}

InetAddr InetAddr::create(const in_addr_t addr)
{
    return InetAddr(InetAddrImpl::create(addr));
}

InetAddr InetAddr::create(const in6_addr& addr)
{
    return InetAddr(InetAddrImpl::create(addr));
}

InetAddr InetAddr::create(const std::string addr)
{
    return InetAddr(InetAddrImpl::create(addr));
}

void InetAddr::setSockAddrStorage(
        sockaddr_storage& storage,
        const int         port,
        const int         sockType) const noexcept
{
    return pImpl->setSockAddrStorage(storage, port, sockType);
}

size_t InetAddr::hash() const noexcept
{
    return pImpl->hash();
}

bool InetAddr::operator <(const InetAddr& that) const noexcept
{
    return *pImpl.get() < *that.pImpl.get();
}

/**
 * Returns the string representation of the Internet address.
 * @return The string representation of the Internet address
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
std::string InetAddr::to_string() const
{
    return pImpl->to_string();
}

int InetAddr::getSocket(const int sockType) const
{
    return pImpl->getSocket(sockType);
}

void InetAddr::setHopLimit(
        const int      sd,
        const unsigned limit) const
{
    pImpl->setHopLimit(sd, limit);
}

void InetAddr::setMcastLoop(
        const int      sd,
        const bool     enable) const
{
    pImpl->setMcastLoop(sd, enable);
}

} // namespace
