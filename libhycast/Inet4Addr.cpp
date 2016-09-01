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

namespace hycast {

Inet4Addr::Inet4Addr(const std::string ipAddr)
    : addr{inet_addr(ipAddr.data())}
{
    if (addr == (in_addr_t)-1)
        throw std::invalid_argument("Invalid IPv4 address: \"" + ipAddr + "\"");

}

int Inet4Addr::compare(const Inet4Addr& that) const
{
    in_addr_t a1 = ntohl(addr);
    in_addr_t a2 = ntohl(that.addr);
    return (a1 < a2)
            ? -1
            : a1 == a2
              ? 0
              : 1;
}

std::string Inet4Addr::to_string() const
{
    char buf[INET_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET, &addr, buf, sizeof(buf)));
}

} // namespace
