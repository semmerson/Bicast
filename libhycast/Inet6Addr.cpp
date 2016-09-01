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

namespace hycast {

/**
 * Constructs from nothing. The IPv6 address will be "::".
 */
Inet6Addr::Inet6Addr()
{
    (void)memset(&addr, 0, sizeof(addr));
}

/**
 * Constructs from a string representation of an IPv6 address.
 * @param[in] ipAddr  A string representation of an IPv6 address
 */
Inet6Addr::Inet6Addr(const std::string ipAddr)
{
    if (inet_pton(AF_INET6, ipAddr.data(), &addr) != 1)
        throw std::invalid_argument("Invalid IPv6 address: \"" + ipAddr +
                "\"");
}

/**
 * Constructs from an IPv6 address.
 * @param[in] ipAddr  An IPv6 address
 */
Inet6Addr::Inet6Addr(const struct in6_addr& ipAddr)
     : addr(ipAddr)
{
}

/**
 * Returns this instance's hash code.
 * @return This instance's hash code.
 */
size_t Inet6Addr::hash() const
{
    size_t hashcode = 0;
    for (const size_t* chunk = (const size_t*)(addr.s6_addr);
            chunk < (const size_t*)(addr.s6_addr + sizeof(addr.s6_addr));
            ++chunk)
        hashcode ^= std::hash<size_t>()(*chunk);
    return hashcode;
}

/**
 * Compares this instance with another.
 * @param that  Another instance
 * @retval <0  This instance is less than the other
 * @retval  0  This instance is equal to the other
 * @retval >0  This instance is greater than the other
 */
int Inet6Addr::compare(const Inet6Addr& that) const
{
    return memcmp(&addr, &that.addr, sizeof(addr));
}

/**
 * Returns a string representation of the IPv6 address.
 * @return A string representation of the IPv6 address.
 */
std::string Inet6Addr::to_string() const
{
    char buf[INET6_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET6, &addr.s6_addr, buf, sizeof(buf)));
}

} // namespace
