/**
 * This file declares a handle class for an immutable Internet address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddr.h
 * @author: Steven R. Emmerson
 */

#ifndef INETADDR_H_
#define INETADDR_H_

#include <memory>
#include <netinet/in.h>
#include <set>
#include <string>
#include <sys/socket.h>

namespace hycast {

class InetAddrImpl; // Forward declaration of implementation

class InetAddr final
{
    std::shared_ptr<InetAddrImpl> pImpl;
    /**
     * Constructs from an implementation.
     * @param[in] impl  implementation
     */
    InetAddr(InetAddrImpl* impl);
    /**
     * Factory method that returns a new instance based on an IPv4 address.
     * @param[in] addr  IPv4 address in network byte order
     * @return A new instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if the string representation is invalid
     * @exceptionsafety Strong guarantee
     * @threadsafety    Thread-safe
     */
    static InetAddr create(const in_addr_t addr);
    /**
     * Factory method that returns a new instance based on an IPv6 address.
     * @param[in] addr  IPv6 address
     * @return A new instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if the string representation is invalid
     * @exceptionsafety Strong guarantee
     * @threadsafety    Thread-safe
     */
    static InetAddr create(const struct in6_addr& addr);
    /**
     * Returns a new instance based on a string specification of an Internet
     * address.
     * @param[in] addr  Internet address specification. Can be a hostname, an
     *                  IPv4 specification, or an IPv6 specification.
     * @return An Internet address instance
     */
    static InetAddr create(const std::string addr);

public:
    /**
     * Constructs from an IPv4 address.
     * @param[in] addr  IPv4 address in network byte order
     */
    explicit InetAddr(const in_addr_t addr)
        : pImpl{create(addr).pImpl} {}

    /**
     * Constructs from an IPv6 address.
     * @param[in] addr  IPv6 address
     */
    explicit InetAddr(const struct in6_addr& addr)
        : pImpl{create(addr).pImpl} {}

    /**
     * Constructs from an Internet address string.
     */
    explicit InetAddr(const std::string addr = "")
        : pImpl{create(addr).pImpl} {}

    /**
     * Returns the `struct sockaddr_storage` corresponding to this instance and
     * a port number.
     * @param[out] storage   Storage structure
     * @param[in]  port      Port number in host byte order
     * @param[in]  sockType  Socket-type hint as for `socket()`. 0 =>
     *                       unspecified.
     * @exceptionsafety      Nothrow
     * @threadsafety         Safe
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
     * Indicates if this instance is considered less than another.
     * @param[in] that  Other instance
     * @retval `true`   Iff this instance is considered less than the other
     */
    bool operator<(const InetAddr& that) const noexcept;
    /**
     * Returns the string representation of the Internet address.
     * @return The string representation of the Internet address
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
     * @returns  This instance
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

/**
 * Indicates if two Internet addresses are considered equal.
 * @param[in] that  Other instance
 * @retval `true`   Iff this instance is considered equal to the other
 */
inline bool operator==(
        const InetAddr& o1,
        const InetAddr& o2) noexcept
{
    return !(o1 < o2) && !(o2 < o1);
}

} // namespace

#endif /* INETADDR_H_ */
