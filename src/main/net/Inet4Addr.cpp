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
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

std::string Inet4Addr::to_string() const
{
    char buf[INET_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET, &ipAddr, buf, sizeof(buf)));
}

std::shared_ptr<std::set<struct sockaddr_storage>> Inet4Addr::getSockAddr(
        const in_port_t  port) const
{
    struct sockaddr_storage sockAddr = {};
    struct sockaddr_in* const addr =
            reinterpret_cast<struct sockaddr_in*>(&sockAddr);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = ipAddr;
    auto set = new std::set<struct sockaddr_storage>();
    if (set == nullptr)
        throw std::system_error(errno, std::system_category(),
                "Couldn't allocate set for socket address");
    set->insert(sockAddr);
    return std::shared_ptr<std::set<struct sockaddr_storage>>{set};
}

} // namespace
