/**
 * This file declares an immutable IPv6 address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Inet6Addr.h
 * @author: Steven R. Emmerson
 */

#ifndef INET6ADDR_H_
#define INET6ADDR_H_

#include "InetAddrImpl.h"
#include "IpAddr.h"

#include <cstddef>
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>

namespace hycast {

class Inet6Addr final : public IpAddr {
    struct in6_addr addr;
public:
    /**
     * Constructs from an IPv6 address.
     * @param[in] ipAddr  IPv6 address
     */
    explicit Inet6Addr(const struct in6_addr& ipAddr)
     : addr(ipAddr) {}
    /**
     * Returns a string representation of the IPv6 address.
     * @return A string representation of the IPv6 address.
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
     * @param[in] port  Port number in host byte order
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

#endif /* INET6ADDR_H_ */
