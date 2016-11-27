/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Inet6Addr.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an IPv6 address.
 */

#include "Inet6Addr.h"

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

std::string Inet6Addr::to_string() const
{
    char buf[INET6_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET6, &ipAddr.s6_addr, buf, sizeof(buf)));
}

std::shared_ptr<std::set<struct sockaddr>> Inet6Addr::getSockAddr(
            const in_port_t  port) const
{
    struct sockaddr sockAddr = {};
    struct sockaddr_in6* const addr =
            reinterpret_cast<struct sockaddr_in6*>(&sockAddr);
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    addr->sin6_addr = ipAddr;
    auto set = new std::set<struct sockaddr>();
    if (set == nullptr)
        throw std::system_error(errno, std::system_category(),
                "Couldn't allocate set for socket address");
    set->insert(sockAddr);
    return std::shared_ptr<std::set<struct sockaddr>>{set};
}

} // namespace
