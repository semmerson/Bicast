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

#include "Inet4Addr.h"
#include "InetAddrImpl.h"
#include "InetNameAddr.h"
#include "IpAddr.h"

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <netinet/in.h>
#include <set>
#include <sys/socket.h>

namespace hycast {

class Inet6Addr final : public IpAddr {
    struct in6_addr ipAddr;
public:
    /**
     * Constructs from an IPv6 address.
     * @param[in] ipAddr  IPv6 address
     */
    explicit Inet6Addr(const struct in6_addr& ipAddr)
     : ipAddr(ipAddr) {}
    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t hash() const noexcept;
    /**
     * Indicates if this instance is considered less than an Internet address.
     * @param[in] that  Internet address
     * @retval `true`   Iff this instance is considered less than the Internet
     *                  address
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
    bool operator<(const Inet4Addr& that) const noexcept
    {
        return less(*this, that);
    }
    /**
     * Indicates if this instance is considered less than an IPv6 address.
     * @param[in] that  IPv6 address
     * @retval `true`   Iff this instance is considered less than the IPv6
     *                  address
     */
    bool operator<(const Inet6Addr& that) const noexcept
    {
        return ::memcmp(ipAddr.s6_addr, that.ipAddr.s6_addr,
                sizeof(ipAddr.s6_addr)) < 0;
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
     * Returns a string representation of the IPv6 address.
     * @return A string representation of the IPv6 address.
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

#endif /* INET6ADDR_H_ */
