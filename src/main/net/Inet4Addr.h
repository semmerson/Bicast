/**
 * This file declares an immutable IPv4 address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Inet4Addr.h
 * @author: Steven R. Emmerson
 */

#ifndef INET4ADDR_H_
#define INET4ADDR_H_

#include "InetAddrImpl.h"
#include "IpAddr.h"

#include <cstddef>
#include <functional>
#include <netinet/in.h>
#include <set>

namespace hycast {

class Inet4Addr final : public IpAddr {
    in_addr_t ipAddr;
public:
    /**
     * Constructs from an IPv4 address.
     * @param[in] ipAddr  IPv4 address
     */
    explicit Inet4Addr(const in_addr_t ipAddr) noexcept : ipAddr(ipAddr) {};
    /**
     * Constructs from an IPv4 address.
     * @param[in] ipAddr  IPv4 address
     * @exceptionsafety Nothrow
     */
    explicit Inet4Addr(const struct in_addr& ipAddr) noexcept
        : ipAddr{ipAddr.s_addr} {};
    /**
     * Returns the string representation of the IPv4 address.
     * @return The string representation of the IPv4 address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const;
    /**
     * Gets the socket addresses corresponding to a port number.
     * @param[in]  port Port number
     * @return     Set of socket addresses
     * @throws std::system_error if the IP address couldn't be obtained
     * @throws std::system_error if required memory couldn't be allocated
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    std::shared_ptr<std::set<struct sockaddr>> getSockAddr(
            const in_port_t  port) const;
};

} // namespace

#endif /* INET4ADDR_H_ */
