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
#include <system_error>

namespace hycast {

/**
 * Constructs from nothing. The IPv6 address will correspond to "::".
 * @exceptionsafety Nothrow
 */
Inet6Addr::Inet6Addr() noexcept
{
    (void)memset(&addr, 0, sizeof(addr));
}

/**
 * Constructs from a string representation of an IPv6 address.
 * @param[in] ipAddr  A string representation of an IPv6 address
 * @throws std::invalid_argument if the string representation is invalid
 * @exceptionsafety Strong
 */
Inet6Addr::Inet6Addr(const std::string ipAddr)
{
    if (inet_pton(AF_INET6, ipAddr.data(), &addr) != 1)
        throw std::invalid_argument("Invalid IPv6 address: \"" + ipAddr +
                "\"");
}

/**
 * Constructs from an IPv6 address.
 * @param[in] ipAddr  IPv6 address
 * @exceptionsafety Nothrow
 */
Inet6Addr::Inet6Addr(const struct in6_addr& ipAddr) noexcept
     : addr(ipAddr)
{
}

/**
 * Returns this instance's hash code.
 * @return This instance's hash code.
 * @exceptionsafety Nothrow
 */
size_t Inet6Addr::hash() const noexcept
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
 * @exceptionsafety Nothrow
 */
int Inet6Addr::compare(const Inet6Addr& that) const noexcept
{
    return memcmp(&addr, &that.addr, sizeof(addr));
}

/**
 * Returns a string representation of the IPv6 address.
 * @return A string representation of the IPv6 address.
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
std::string Inet6Addr::to_string() const
{
    char buf[INET6_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET6, &addr.s6_addr, buf, sizeof(buf)));
}

void Inet6Addr::connect(
        int       sd,
        in_port_t port) const
{
    struct sockaddr_in6 sockAddr;
    (void)memset((void*)&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin6_family = AF_INET6;
    sockAddr.sin6_port = htons(port);
    sockAddr.sin6_addr = addr;
    int status = ::connect(sd, (struct sockaddr*)&sockAddr, sizeof(sockAddr));
    if (status)
        throw std::system_error(errno, std::system_category(),
                "connect() failure: socket=" + std::to_string(sd) +
                ", addr=" + to_string());
}

void Inet6Addr::bind(
        int       sd,
        in_port_t port) const
{
    struct sockaddr_in6 sockAddr;
    (void)memset((void*)&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin6_family = AF_INET6;
    sockAddr.sin6_port = htons(port);
    sockAddr.sin6_addr = addr;
    int status = ::bind(sd, (struct sockaddr*)&sockAddr, sizeof(sockAddr));
    if (status)
        throw std::system_error(errno, std::system_category(),
                "bind() failure: socket=" + std::to_string(sd) +
                ", addr=" + to_string());
}

} // namespace
