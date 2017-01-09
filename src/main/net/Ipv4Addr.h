/**
 * This file declares an immutable IPv4 address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Ipv4Addr.h
 * @author: Steven R. Emmerson
 */

#ifndef INET4ADDR_H_
#define INET4ADDR_H_

#include "InetAddrImpl.h"
#include "IpAddr.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <netinet/in.h>
#include <set>

namespace hycast {

class Ipv4Addr final : public IpAddr {
    in_addr_t ipAddr;
public:
    /**
     * Constructs from an IPv4 address.
     * @param[in] ipAddr  IPv4 address
     */
    explicit Ipv4Addr(const in_addr_t ipAddr) noexcept : ipAddr(ipAddr) {};
    /**
     * Constructs from an IPv4 address.
     * @param[in] ipAddr  IPv4 address
     * @exceptionsafety Nothrow
     */
    explicit Ipv4Addr(const struct in_addr& ipAddr) noexcept
        : ipAddr{ipAddr.s_addr} {};
    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t hash() const noexcept
    {
        std::hash<uint32_t> h;
        return h(ipAddr);
    }
    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  Other instance
     * @retval `true`   Iff this instance is considered less than the other
     */
    bool operator<(const InetAddrImpl& that) const noexcept
    {
        return !(that < *this);
    }
    /**
     * Indicates if this instance is considered less than an IPv4 address.
     * @param[in] that  IPv4 address
     * @retval `true`   Iff this instance is considered less than the IPv4
     *                  address
     */
    bool operator<(const Ipv4Addr& that) const noexcept
    {
        return ::ntohl(ipAddr) < ::ntohl(that.ipAddr);
    }
    /**
     * Indicates if this instance is considered less than an IPv6 address.
     * @param[in] that  IPv6 address
     * @retval `true`   Iff this instance is considered less than the IPv6
     *                  address
     */
    bool operator<(const Ipv6Addr& that) const noexcept
    {
        return less(*this, that);
    }
    /**
     * Indicates if this instance is considered less than a hostname address.
     * @param[in] that  Hostname address
     * @retval `true`   Iff this instance is considered less than the hostname
     *                  address
     */
    bool operator<(const InetNameAddr& that) const noexcept
    {
        return less(*this, that);
    }
    /**
     * Returns the string representation of the IPv4 address.
     * @return The string representation of the IPv4 address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const;
    /**
     * Gets the socket addresses corresponding to a port number.
     * @param[in]  port      Port number
     * @return     Set of socket addresses
     * @throws std::system_error if the IP address couldn't be obtained
     * @throws std::system_error if required memory couldn't be allocated
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    std::shared_ptr<std::set<struct sockaddr_storage>> getSockAddr(
            const in_port_t  port) const;
};

} // namespace

#endif /* INET4ADDR_H_ */
