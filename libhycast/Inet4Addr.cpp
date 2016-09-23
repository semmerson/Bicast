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
#include <system_error>
#include <sys/socket.h>

namespace hycast {

/**
 * Constructs from a string representation of an IPv4 address.
 * @param[in] ipAddr  A string representation of an IPv4 address
 * @throws std::invalid_argument if the string represents an invalid IPv4
 *                               address
 * @exceptionsafety Strong
 */
Inet4Addr::Inet4Addr(const std::string ipAddr)
    : addr{inet_addr(ipAddr.data())}
{
    if (addr == (in_addr_t)-1)
        throw std::invalid_argument("Invalid IPv4 address: \"" + ipAddr + "\"");
}

/**
 * Compares this instance with another.
 * @param that  Another instance
 * @retval <0  This instance is less than the other
 * @retval  0  This instance is equal to the other
 * @retval >0  This instance is greater than the other
 * @exceptionsafety Nothrow
 */
int Inet4Addr::compare(const Inet4Addr& that) const noexcept
{
    in_addr_t a1 = ntohl(addr);
    in_addr_t a2 = ntohl(that.addr);
    return (a1 < a2)
            ? -1
            : a1 == a2
              ? 0
              : 1;
}

/**
 * Returns a string representation of the IPv4 address.
 * @return A string representation of the IPv4 address.
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
std::string Inet4Addr::to_string() const
{
    char buf[INET_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET, &addr, buf, sizeof(buf)));
}

void Inet4Addr::connect(
        const int       sd,
        const in_port_t port) const
{
    struct sockaddr_in sockAddr;
    (void)memset((void*)&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(port);
    sockAddr.sin_addr.s_addr = addr;
    int status = ::connect(sd, (struct sockaddr*)&sockAddr, sizeof(sockAddr));
    if (status)
        throw std::system_error(errno, std::system_category(),
                "connect() failure: socket=" + std::to_string(sd) +
                ", addr=" + to_string());
}

void Inet4Addr::bind(
        const int       sd,
        const in_port_t port) const
{
    struct sockaddr_in sockAddr;
    (void)memset((void*)&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(port);
    sockAddr.sin_addr.s_addr = addr;
    int status = ::bind(sd, (struct sockaddr*)&sockAddr, sizeof(sockAddr));
    if (status)
        throw std::system_error(errno, std::system_category(),
                "bind() failure: socket=" + std::to_string(sd) +
                ", addr=" + to_string());
}

} // namespace
