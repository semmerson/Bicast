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

#include <cstddef>
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>

namespace hycast {

class Inet6Addr final : public InetAddrImpl {
    struct in6_addr addr;
public:
    /**
     * Constructs from nothing. The IPv6 address will correspond to "::".
     * @exceptionsafety Nothrow
     */
    Inet6Addr() noexcept;
    /**
     * Constructs from a string representation of an IPv6 address.
     * @param[in] ipAddr  A string representation of an IPv6 address
     * @throws std::invalid_argument if the string representation is invalid
     * @exceptionsafety Strong
     */
    explicit Inet6Addr(const std::string ipAddr);
    /**
     * Constructs from an IPv6 address.
     * @param[in] ipAddr  IPv6 address
     * @exceptionsafety Nothrow
     */
    explicit Inet6Addr(const struct in6_addr& ipAddr) noexcept;
    /**
     * Returns the address family.
     * @retval AF_INET6
     * @exceptionsafety Nothrow
     */
    int get_family() const noexcept {return AF_INET6;}
    /**
     * Returns this instance's hash code.
     * @return This instance's hash code.
     * @exceptionsafety Nothrow
     */
    size_t hash() const noexcept;
    /**
     * Compares this instance with another.
     * @param that  Another instance
     * @retval <0  This instance is less than the other
     * @retval  0  This instance is equal to the other
     * @retval >0  This instance is greater than the other
     * @exceptionsafety Nothrow
     */
    int compare(const Inet6Addr& that) const noexcept;
    /**
     * Indicates if this instance equals another.
     * @param that  Other instance
     * @retval `true`   This instance equals the other
     * @retval `false`  This instance doesn't equal the other
     * @exceptionsafety Nothrow
     */
    int equals(const Inet6Addr& that) const noexcept {return compare(that) == 0;}
    /**
     * Returns a string representation of the IPv6 address.
     * @return A string representation of the IPv6 address.
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const;
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
