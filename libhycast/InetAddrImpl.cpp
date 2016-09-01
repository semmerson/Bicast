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
#include "InetAddrImpl.h"

#include "arpa/inet.h"

namespace hycast {

std::shared_ptr<InetAddrImpl> InetAddrImpl::create()
{
    return std::shared_ptr<InetAddrImpl>(new Inet4Addr());
}

std::shared_ptr<InetAddrImpl> InetAddrImpl::create(const std::string ip_addr)
{
    in_addr_t ipv4_addr;
    if (inet_pton(AF_INET, ip_addr.data(), &ipv4_addr) == 1) {
        return std::shared_ptr<InetAddrImpl>(new Inet4Addr(ipv4_addr));
    }
    else {
        struct in6_addr ipv6_addr;
        if (inet_pton(AF_INET6, ip_addr.data(), &ipv6_addr) != 1) {
            throw std::invalid_argument("Invalid IP address: \"" + ip_addr +
                "\"");
        }
        else {
            return std::shared_ptr<InetAddrImpl>(new Inet6Addr(ipv6_addr));
        }
    }
}

int InetAddrImpl::compare(const InetAddrImpl& that) const
{
    if (get_family() == AF_INET) {
        return (that.get_family() == AF_INET)
            ? static_cast<const Inet4Addr*>(this)->compare(
                    static_cast<const Inet4Addr&>(that))
            : -1;
    }
    return (that.get_family() == AF_INET6)
        ? static_cast<const Inet6Addr*>(this)->compare(
                static_cast<const Inet6Addr&>(that))
        : 1;
}

} // namespace
