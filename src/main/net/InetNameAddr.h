/**
 * This file declares an immutable IPv4 address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetNameAddr.h
 * @author: Steven R. Emmerson
 */

#ifndef INETNAMEADDR_H_
#define INETNAMEADDR_H_

#include "Inet4Addr.h"
#include "InetAddrImpl.h"
#include "IpAddr.h"

#include <cstddef>
#include <memory>
#include <netinet/in.h>
#include <set>

namespace hycast {

class InetNameAddr final : public InetAddrImpl {
    std::string name;
    /**
     * Adds Internet addresses to a set.
     * @param[in]  family             Internet address family of addresses to add
     * @param[in]  port               Port number in host byte order
     * @param[out] set                Set of Internet addresses.
     * @throws std::system_error      The IP address couldn't be obtained
     * @exceptionsafety               Strong guarantee
     * @threadsafety                  Safe
     */
    void getSockAddrs(
            const int                                family,
            const in_port_t                          port,
            std::set<struct sockaddr_storage>* const set) const;
public:
    /**
     * Constructs from a hostname.
     * @param[in] name  A hostname
     */
    explicit InetNameAddr(const std::string name);
    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t hash() const noexcept
    {
        std::hash<std::string> h;
        return h(name);
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
        return name < that.name;
    }
    /**
     * Returns the hostname.
     * @return The hostname
     * @exceptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    std::string to_string() const noexcept;
    /**
     * Gets the socket addresses corresponding to an Internet protocol and a
     * port number.
     * @param[in]  port      Port number in host byte order
     * @return     Set of socket addresses
     * @throws std::system_error if the IP address couldn't be obtained
     * @throws std::system_error if required memory couldn't be allocated
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    virtual std::shared_ptr<std::set<struct sockaddr_storage>> getSockAddr(
            const in_port_t  port) const;
};

} // namespace

#endif /* INETNAMEADDR_H_ */
