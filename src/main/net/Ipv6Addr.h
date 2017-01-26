/**
 * This file declares an immutable IPv6 address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Ipv6Addr.h
 * @author: Steven R. Emmerson
 */

#ifndef INET6ADDR_H_
#define INET6ADDR_H_

#include "InetAddrImpl.h"
#include "InetNameAddr.h"
#include "Ipv4Addr.h"

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <netinet/in.h>
#include <set>
#include <sys/socket.h>
#include "IpAddrImpl.h"

namespace hycast {

class Ipv6Addr final : public IpAddrImpl {
    struct in6_addr ipAddr;
public:
    /**
     * Constructs from an IPv6 address.
     * @param[in] ipAddr  IPv6 address
     */
    explicit Ipv6Addr(const struct in6_addr& ipAddr)
     : ipAddr(ipAddr) {}

    /**
     * Returns the `struct sockaddr_storage` corresponding to this instance and
     * a port number.
     * @param[out] storage   Storage structure. Set upon return.
     * @param[in]  port      Port number in host byte order
     * @param[in]  sockType  Ignored. Socket type hint as for `socket()`.
     *                       0 => unspecified.
     * @return Socket address as a `struct sockaddr_storage`
     * @exceptionsafety  Nothrow
     * @threadsafety     Safe
     */
    void setSockAddrStorage(
            sockaddr_storage& storage,
            const int         port,
            const int         sockType = 0) const noexcept;

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
    bool operator<(const Ipv4Addr& that) const noexcept
    {
        return less(*this, that);
    }
    /**
     * Indicates if this instance is considered less than an IPv6 address.
     * @param[in] that  IPv6 address
     * @retval `true`   Iff this instance is considered less than the IPv6
     *                  address
     */
    bool operator<(const Ipv6Addr& that) const noexcept
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
     * Returns a new socket.
     * @param[in] sockType  Type of socket as defined in <sys/socket.h>:
     *                        - SOCK_STREAM     Streaming socket (e.g., TCP)
     *                        - SOCK_DGRAM      Datagram socket (e.g., UDP)
     *                        - SOCK_SEQPACKET  Record-oriented socket
     * @return Corresponding new socket
     * @throws std::system_error  `socket()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    int getSocket(const int sockType) const;

    /**
     * Sets the hop-limit on a socket for outgoing multicast packets.
     * @param[in] sd     Socket
     * @param[in] limit  Hop limit:
     *                     -         0  Restricted to same host. Won't be
     *                                  output by any interface.
     *                     -         1  Restricted to the same subnet. Won't
     *                                  be forwarded by a router (default).
     *                     -    [2,31]  Restricted to the same site,
     *                                  organization, or department.
     *                     -   [32,63]  Restricted to the same region.
     *                     -  [64,127]  Restricted to the same continent.
     *                     - [128,255]  Unrestricted in scope. Global.
     * @throws std::system_error  `setsockopt()` failure
     * @execptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setHopLimit(
            const int      sd,
            const unsigned limit) const;

    /**
     * Sets whether or not a multicast packet written to a socket will be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] sd      Socket descriptor
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @return  This instance
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setMcastLoop(
            const int  sd,
            const bool enable) const;
};

} // namespace

#endif /* INET6ADDR_H_ */
