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

#include "Inet4Addr.h"
#include "Inet6Addr.h"
#include "InetAddr.h"
#include "InetAddrImpl.h"

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

std::shared_ptr<std::set<struct sockaddr>> InetAddr::getSockAddr(
        const in_port_t  port) const
{
    return pImpl->getSockAddr(port);
}

} // namespace
