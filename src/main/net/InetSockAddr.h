/**
 * This file declares an immutable Internet socket address, which comprises an
 * Internet address and a port number.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetSockAddr.h
 * @author: Steven R. Emmerson
 */

#ifndef INETSOCKADDR_H_
#define INETSOCKADDR_H_

#include "InetAddr.h"
#include "PortNumber.h"

#include <memory>
#include <netinet/in.h>
#include <string>

namespace hycast {

class InetSockAddrImpl; // Forward declaration

class InetSockAddr final {
    std::shared_ptr<InetSockAddrImpl> pImpl;
public:
    /**
     * Constructs from nothing. The resulting object will have the default
     * Internet address of the host and port number 0.
     * @throws std::bad_alloc if necessary memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddr();

    /**
     * Constructs from an Internet address and a port number.
     * @param[in] inetAddr Internet address
     * @param[in] port     Port number
     * @throws std::bad_alloc if necessary memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddr(
            const InetAddr   inetAddr,
            const in_port_t  port);

    /**
     * Constructs from a string representation of an IP address and a port
     * number.
     * @param[in] ip_addr  IP address (either IPv4 or IPv6)
     * @param[in] port     Port number
     * @throws std::invalid_argument if the IP address is invalid
     * @throws std::bad_alloc if necessary memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddr(
            const std::string ip_addr,
            const in_port_t   port);

    /**
     * Constructs from an IPv4 address.
     * @param[in] addr  IPv4 address in _network_ byte order
     * @param[in] port  Port number in _host_ bytes order
     * @throws std::bad_alloc if necessary memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddr(
            const in_addr_t  addr,
            const PortNumber port);

    /**
     * Constructs from an IPv4 socket address.
     * @param[in] addr  IPv4 socket address
     * @throws std::bad_alloc if necessary memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddr(const struct sockaddr_in& addr);

    /**
     * Constructs from an IPv6 address.
     * @param[in] addr  IPv6 address in _network_ byte order
     * @param[in] port  Port number in _host_ bytes order
     * @throws std::bad_alloc if necessary memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddr(
            const struct in6_addr& addr,
            const in_port_t        port);

    /**
     * Constructs from an IPv6 socket address.
     * @param[in] addr  IPv6 socket address
     * @throws std::bad_alloc if necessary memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddr(const struct sockaddr_in6& sockaddr);

    /**
     * Constructs from a generic socket address.
     * @param[in] addr  Generic socket address
     */
    InetSockAddr(const struct sockaddr& sockaddr);

    /**
     * Copy constructs from another instance.
     * @param[in] that  Other instance
     * @exceptionsafety Nothrow
     */
    InetSockAddr(const InetSockAddr& that) noexcept;

    /**
     * Move constructs.
     * @param[in] that  Other instance
     */
    InetSockAddr(InetSockAddr&& that) noexcept =default;

    /**
     * Copy assigns from an instance.
     * @param[in] rhs  An instance
     * @exceptionsafety Nothrow
     */
    InetSockAddr& operator=(const InetSockAddr& rhs) noexcept;

    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @exceptionsafety Nothrow
     */
    size_t hash() const noexcept;
    /**
     * Indicates if this instance is considered less than another
     * @param[in] that  Other instance
     * @retval `true`   Iff this instance is less than the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator<(const InetSockAddr& that) const noexcept;

    /**
     * Returns a string representation of this instance.
     * @return A string representation of this instance
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
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    int getSocket(const int sockType) const;

    /**
     * Connects a socket to this instance's endpoint.
     * @param[in] sd        Socket descriptor
     * @returns  This instance
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    const InetSockAddr& connect(int sd) const;

    /**
     * Binds a socket's local endpoint to this instance.
     * @param[in] sd        Socket descriptor
     * @returns  This instance
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    const InetSockAddr& bind(int sd) const;

    /**
     * Returns the Internet socket address that's suitable for a client.
     * @return Internet socket address that's suitable for a client
     */
    InetSockAddr getClntSockAddr() const;

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
    const InetSockAddr& setHopLimit(
            const int      sd,
            const unsigned limit) const;

    /**
     * Sets whether or not a multicast packet sent to a socket will also be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] sd      Socket descriptor
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @return  This instance
     * @threadsafety  Safe
     */
    const InetSockAddr& setMcastLoop(
            const int  sd,
            const bool enable) const;

    /**
     * Joins a socket to the multicast group corresponding to this instance.
     * @param[in] sd  Socket descriptor
     * @returns  This instance
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    const InetSockAddr& joinMcastGroup(const int sd) const;

    /**
     * Joins a socket to the source-specific multicast group corresponding to
     * this instance and the IP address of the source.
     * @param[in] sd       Socket descriptor
     * @param[in] srcAddr  IP address of source
     * @return             This instance
     * @throws std::system_error  `setsockopt()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    const InetSockAddr& joinSourceGroup(
            const int       sd,
            const InetAddr& srcAddr) const;
};

/**
 * Indicates if one instance equals another.
 * @retval <0  This instance is less than the other
 * @retval  0  This instance is equal to the other
 * @retval >0  This instance is greater than the other
 * @exceptionsafety Nothrow
 */
inline bool operator==(
        const InetSockAddr& o1,
        const InetSockAddr& o2) noexcept
{
    return !(o1 < o2) && !(o2 < o1);
}

} // namespace

#endif /* INETSOCKADDR_H_ */
