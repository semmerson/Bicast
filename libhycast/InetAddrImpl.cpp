/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddrImpl.cpp
 * @author: Steven R. Emmerson
 *
 * This file implements an Internet address.
 */

#include "Inet4Addr.h"
#include "Inet6Addr.h"
#include "InetNameAddr.h"

#include "arpa/inet.h"

namespace hycast {

std::shared_ptr<InetAddrImpl> InetAddrImpl::create(const in_addr_t addr)
{
    return std::shared_ptr<InetAddrImpl>(new Inet4Addr(addr));
}

std::shared_ptr<InetAddrImpl> InetAddrImpl::create(const struct in6_addr& addr)
{
    return std::shared_ptr<InetAddrImpl>(new Inet6Addr(addr));
}

std::shared_ptr<InetAddrImpl> InetAddrImpl::create(const std::string addr)
{
    in_addr_t ipv4_addr;
    if (inet_pton(AF_INET, addr.data(), &ipv4_addr) == 1) {
        return create(ipv4_addr);
    }
    else {
        struct in6_addr ipv6_addr;
        if (inet_pton(AF_INET6, addr.data(), &ipv6_addr) == 1) {
            return create(ipv6_addr);
        }
        else {
            return std::shared_ptr<InetAddrImpl>(new InetNameAddr(addr));
        }
    }
}

} // namespace
