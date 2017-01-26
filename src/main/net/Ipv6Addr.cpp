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
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

void Ipv6Addr::setSockAddrStorage(
        sockaddr_storage& storage,
        const int         port,
        const int         sockType) const noexcept
{
    ::memset(&storage, 0, sizeof(storage));
    struct sockaddr_in6* sockAddr =
            reinterpret_cast<struct sockaddr_in6*>(&storage);
    sockAddr->sin6_family = AF_INET6;
    sockAddr->sin6_port = htons(port);
    sockAddr->sin6_addr = ipAddr;
}

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

int Ipv6Addr::getSocket(const int sockType) const
{
    int sd = ::socket(AF_INET6, sockType, 0);
    if (sd == -1)
        throw std::system_error(errno, std::system_category(),
                "socket() failure: sockType=" + std::to_string(sockType));
    return sd;
}

void Ipv6Addr::setHopLimit(
        const int      sd,
        const unsigned limit) const
{
    if (limit > 255)
        throw std::invalid_argument("Invalid hop-limit: " +
                std::to_string(limit));
    const int value = limit;
    if (::setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &value, sizeof(value)))
        throw std::system_error(errno, std::system_category(),
                std::string("Couldn't set hop-limit for multicast packets: "
                "sock=") + std::to_string(sd) + ", group=" + to_string() +
                ", limit=" + std::to_string(value));
}

void Ipv6Addr::setMcastLoop(
        const int  sd,
        const bool enable) const
{
    const unsigned value = enable;
    if (::setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &value,
            sizeof(value)))
        throw std::system_error(errno, std::system_category(),
                std::string("Couldn't set multicast packet looping: "
                "sock=") + std::to_string(sd) + ", enable=" +
                std::to_string(enable));
}

} // namespace
