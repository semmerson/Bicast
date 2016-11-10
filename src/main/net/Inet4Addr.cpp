/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Inet4Addr.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an IPv4 address.
 */

#include "Inet4Addr.h"

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

std::string Inet4Addr::to_string() const
{
    char buf[INET_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET, &addr, buf, sizeof(buf)));
}

void Inet4Addr::getSockAddr(
            const in_port_t  port,
            struct sockaddr& sockAddr,
            socklen_t&       sockLen) const noexcept
{
    struct sockaddr_in* const inAddr{reinterpret_cast<struct sockaddr_in*>(&sockAddr)};
    sockLen = sizeof(*inAddr);
    (void)memset(static_cast<void*>(inAddr), 0, sockLen);
    inAddr->sin_family = AF_INET;
    inAddr->sin_port = htons(port);
    inAddr->sin_addr.s_addr = addr;
}

} // namespace
