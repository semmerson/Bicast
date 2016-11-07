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
    return std::string(inet_ntop(AF_INET6, &addr.s6_addr, buf, sizeof(buf)));
}

void Inet6Addr::getSockAddr(
            const in_port_t  port,
            struct sockaddr& sockAddr,
            socklen_t&       sockLen) const noexcept
{
    struct sockaddr_in6* const inAddr{reinterpret_cast<struct sockaddr_in6*>(&sockAddr)};
    sockLen = sizeof(*inAddr);
    (void)memset(static_cast<void*>(inAddr), 0, sockLen);
    inAddr->sin6_family = AF_INET6;
    inAddr->sin6_port = htons(port);
    inAddr->sin6_addr = addr;
}

} // namespace
