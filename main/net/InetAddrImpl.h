/**
 * This file declares an abstract base class for an immutable Internet address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddrImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef INETADDRIMPL_H_
#define INETADDRIMPL_H_

#include "InetSockAddr.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <set>
#include <sys/socket.h>

namespace hycast {

class Ipv4Addr;
class Ipv6Addr;
class InetNameAddr;

class InetAddrImpl {
public:
    /**
     * Factory method that returns a new instance based on an IPv4 address.
     * @param[in] addr  IPv4 address in network byte order
     * @return A new instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if the string representation is invalid
     * @exceptionsafety Strong guarantee
     * @threadsafety    Thread-safe
     */
    static InetAddrImpl* create(const in_addr_t addr);
    /**
     * Factory method that returns a new instance based on an IPv6 address.
     * @param[in] addr  IPv6 address
     * @return A new instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if the string representation is invalid
     * @exceptionsafety Strong guarantee
     * @threadsafety    Thread-safe
     */
    static InetAddrImpl* create(const struct in6_addr& addr);
    /**
     * Factory method that returns a new instance based on the string
     * representation of an Internet address.
     * @param[in] addr  The string representation of an Internet address. Can be
     *                  hostname, IPv4, or IPv6.
     * @return A new instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if the string representation is invalid
     * @exceptionsafety Strong guarantee
     * @threadsafety    Thread-safe
     */
    static InetAddrImpl* create(const std::string addr);
    /**
     * Factory method that returns the default instance.
     * @return          Default instance
     * @exceptionsafety Strong guarantee
     * @threadsafety    Thread-safe
     */
    static InetAddrImpl* create();
    /**
     * Destructor.
     */
    virtual             ~InetAddrImpl() {};

    /**
     * Returns the `struct sockaddr_storage` corresponding to this instance and
     * a port number.
     * @param[out] storage   Storage structure. Set upon return.
     * @param[in]  port      Port number in host byte order
     * @param[in]  sockType  Socket type hint as for `socket()`. 0 => unspecified.
     * @exceptionsafety  Nothrow
     * @threadsafety     Safe
     */
    virtual void setSockAddrStorage(
            sockaddr_storage& storage,
            const int         port,
            const int         sockType = 0) const =0;

    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    virtual size_t hash() const noexcept =0;
    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  Other instance
     * @retval `true`   Iff this instance is considered less than the other
     */
    virtual bool operator<(const InetAddrImpl& that) const noexcept =0;
    /**
     * Indicates if this instance is considered less than an IPv4 address.
     * @param[in] that  IPv4 address
     * @retval `true`   Iff this instance is considered less than the IPv4
     *                  address
     */
    virtual bool operator<(const Ipv4Addr& that) const noexcept =0;
    /**
     * Indicates if this instance is considered less than an IPv6 address.
     * @param[in] that  IPv6 address
     * @retval `true`   Iff this instance is considered less than the IPv6
     *                  address
     */
    virtual bool operator<(const Ipv6Addr& that) const noexcept =0;
    /**
     * Indicates if this instance is considered less than a hostname address.
     * @param[in] that  Hostname address
     * @retval `true`   Iff this instance is considered less than the hostname
     *                  address
     */
    virtual bool operator<(const InetNameAddr& that) const noexcept =0;
    /**
     * Returns the string representation of the Internet address.
     * @return The string representation of the Internet address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    virtual std::string to_string() const = 0;

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
    virtual int getSocket(const int sockType) const =0;

    /**
     * Sets the interface to use for outgoing datagrams.
     * @param[in] inetAddr  Internet address of interface
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    virtual void setInterface(const int sd) const =0;

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
    virtual void setHopLimit(
            const int      sd,
            const unsigned limit) const =0;

    /**
     * Sets whether or not a multicast packet written to a socket will be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] sd      Socket descriptor
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @return  This instance
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    virtual void setMcastLoop(
            const int  sd,
            const bool enable) const =0;
};

inline bool less(const Ipv4Addr& o1, const Ipv6Addr& o2) noexcept
{
    return true;
}

inline bool less(const Ipv4Addr& o1, const InetNameAddr& o2) noexcept
{
    return true;
}

inline bool less(const Ipv6Addr& o1, const InetNameAddr& o2) noexcept
{
    return true;
}

inline bool less(const Ipv6Addr& o1, const Ipv4Addr& o2) noexcept
{
    return false;
}

inline bool less(const InetNameAddr& o1, const Ipv4Addr& o2) noexcept
{
    return false;
}

inline bool less(const InetNameAddr& o1, const Ipv6Addr& o2) noexcept
{
    return false;
}

} // namespace

namespace std {
    template<>
    struct less<struct sockaddr_storage> {
        bool operator()(
            const struct sockaddr_storage sockaddr1,
            const struct sockaddr_storage sockaddr2)
        {
            return ::memcmp(&sockaddr1, &sockaddr2, sizeof(sockaddr1)) < 0;
        }
    };
}

#endif /* INETADDRIMPL_H_ */
