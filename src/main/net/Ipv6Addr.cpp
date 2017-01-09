/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Ipv6Addr.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an IPv6 address.
 */

#include "Ipv6Addr.h"

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

size_t Ipv6Addr::hash() const noexcept
{
    const size_t*       ptr = reinterpret_cast<const size_t*>(&ipAddr.s6_addr);
    const size_t* const out = ptr + sizeof(ipAddr.s6_addr)/sizeof(size_t);
    size_t              hash = 0;
    while (ptr < out)
        hash ^= *ptr++;
    return hash;
}

std::string Ipv6Addr::to_string() const
{
    char buf[INET6_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET6, &ipAddr.s6_addr, buf, sizeof(buf)));
}

std::shared_ptr<std::set<struct sockaddr_storage>> Ipv6Addr::getSockAddr(
        const in_port_t  port) const
{
    struct sockaddr_storage sockAddr = {};
    struct sockaddr_in6* const addr =
            reinterpret_cast<struct sockaddr_in6*>(&sockAddr);
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    addr->sin6_addr = ipAddr;
    auto set = new std::set<struct sockaddr_storage>();
    if (set == nullptr)
        throw std::system_error(errno, std::system_category(),
                "Couldn't allocate set for socket address");
    set->insert(sockAddr);
    return std::shared_ptr<std::set<struct sockaddr_storage>>{set};
}

} // namespace
