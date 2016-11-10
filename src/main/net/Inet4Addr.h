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

namespace hycast {

class Inet4Addr final : public IpAddr {
    in_addr_t addr;
public:
    /**
     * Constructs from an IPv4 address.
     * @param[in] ipAddr  IPv4 address
     */
    explicit Inet4Addr(const in_addr_t ipAddr) noexcept : addr(ipAddr) {};
    /**
     * Constructs from an IPv4 address.
     * @param[in] ipAddr  IPv4 address
     * @exceptionsafety Nothrow
     */
    explicit Inet4Addr(const struct in_addr& ipAddr) noexcept : addr{ipAddr.s_addr} {};
    /**
     * Returns the string representation of the IPv4 address.
     * @return The string representation of the IPv4 address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const;
    /**
     * Gets the socket address corresponding to a port number.
     * @param[in]  port      Port number
     * @param[out] sockAddr  Resulting socket address
     * @param[out] sockLen   Size of socket address in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    void getSockAddr(
            const in_port_t  port,
            struct sockaddr& sockAddr,
            socklen_t&       sockLen) const noexcept;
    /**
     * Connects a socket to the given port of this instance's endpoint.
     * @param[in] sd    Socket descriptor
     * @param[in] port  Port number
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void connect(
            int       sd,
            in_port_t port) const;
    /**
     * Binds a socket to the given port of this instance's endpoint.
     * @param[in] sd    Socket descriptor
     * @param[in] port  Port number in host byte order
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void bind(
            int       sd,
            in_port_t port) const;
};

} // namespace

#endif /* INET4ADDR_H_ */
