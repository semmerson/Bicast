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

InetAddr::InetAddr()
    : pImpl{InetAddrImpl::create()}
{
}

InetAddr::InetAddr(const std::string ip_addr)
    : pImpl(InetAddrImpl::create(ip_addr))
{
}

InetAddr::InetAddr(const in_addr_t addr)
    : pImpl{new Inet4Addr(addr)}
{
}

InetAddr::InetAddr(const struct in_addr& addr)
    : pImpl{new Inet4Addr(addr)}
{
}

InetAddr::InetAddr(const struct in6_addr& addr)
    : pImpl{new Inet6Addr(addr)}
{
}

InetAddr::InetAddr(const InetAddr& that)
    : pImpl(that.pImpl)
{
}

int InetAddr::get_family() const
{
    return pImpl->get_family();
}

int InetAddr::compare(const InetAddr& that) const
{
    return pImpl->compare(*that.pImpl.get());
}

InetAddr& InetAddr::operator=(const InetAddr& rhs)
{
    pImpl = rhs.pImpl; // InetAddrImpl class is immutable
    return *this;
}

size_t InetAddr::hash() const
{
    return pImpl->hash();
}

std::string InetAddr::to_string() const
{
    return pImpl->to_string();
}

} // namespace
