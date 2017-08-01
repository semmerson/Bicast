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

#include "InetAddrImpl.h"
#include <cstddef>
#include <memory>
#include <netinet/in.h>
#include <set>

#include "IpAddrImpl.h"
#include "Ipv4Addr.h"

namespace hycast {

class InetNameAddr final : public InetAddrImpl
{
    std::string name; /// Hostname

    /**
     * Adds Internet addresses to a set.
     * @param[in]  family         Internet address family of addresses to add
     * @param[in]  sockType       Type of socket as defined in <sys/socket.h>
     * @param[in]  port           Port number in host byte order
     * @param[out] set            Set of Internet addresses.
     * @throws std::system_error  The IP address couldn't be obtained
     * @exceptionsafety           Strong guarantee
     * @threadsafety              Safe
     */
    void getSockAddrs(
            const int                                family,
            const int                                sockType,
            const in_port_t                          port,
            std::set<struct sockaddr_storage>* const set) const;

    /**
     * Returns the first IP-based Internet address of the given family that's
     * associated with this instance's hostname.
     * @param[in] family  Address family. One of `AF_INET` or `AF_INET6`.
     * @retval NULL  No address found
     * @return       First matching address
     * @throws std::system_error `getaddrinfo()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    IpAddrImpl* getIpAddr(const int family) const;

    /**
     * Returns the first IP-based Internet address associated with this
     * instance's hostname.
     * @retval NULL  No address found
     * @return       First matching address
     * @throws std::system_error `getaddrinfo()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    IpAddrImpl* getIpAddr() const;

public:
    /**
     * Constructs from a hostname.
     * @param[in] name  A hostname
     */
    explicit InetNameAddr(const std::string name);

    /**
     * Returns the `struct sockaddr_storage` corresponding to this instance and
     * a port number.
     * @param[out] storage   Storage structure. Set upon return.
     * @param[in]  port      Port number in host byte order
     * @param[in]  sockType  Socket type hint as for `socket()`. 0 =>
     *                       unspecified.
     * @throw SystemError    ::getaddrinfo() failure
     * @exceptionsafety      Strong Guarantee
     * @threadsafety         Safe
     */
    void setSockAddrStorage(
            sockaddr_storage& storage,
            const int         port,
            const int         sockType = 0) const;

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
     * Sets the interface to use for outgoing datagrams.
     * @param[in] sd     Socket descriptor
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setInterface(const int sd) const;

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

#endif /* INETNAMEADDR_H_ */
