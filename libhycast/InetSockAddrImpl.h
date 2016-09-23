/**
 * This file declares an abstract base class for an immutable Internet socket
 * address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetSockAddrImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef INETSOCKADDRIMPL_H_
#define INETSOCKADDRIMPL_H_

#include "InetAddr.h"
#include "PortNumber.h"

#include <cstddef>
#include <netinet/in.h>
#include <string>

namespace hycast {

class InetSockAddrImpl final {
    InetAddr  inetAddr;
    in_port_t port;  // In host byte-order
public:
    /**
     * Constructs from nothing. The Internet address will correspond to
     * "0.0.0.0" (INET_ANY) and the port number will be 0.
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddrImpl();
    /**
     * Constructs from a string representation of an Internet address and a port
     * number.
     * @param[in] ipAddr  String representation of Internet address
     * @param[in] port    Port number in host byte-order
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if the string representation is invalid
     * @exceptionsafety Strong
     */
    InetSockAddrImpl(
            const std::string ipAddr,
            const in_port_t   port);
    /**
     * Constructs from an IPV4 address and a port number.
     * @param[in] addr  IPv4 address
     * @param[in] port  Port number in host byte-order
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddrImpl(
            const in_addr_t  addr,
            const PortNumber port);
    /**
     * Constructs from an IPV6 address and a port number.
     * @param[in] addr  IPv6 address
     * @param[in] port  Port number in host byte-order
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddrImpl(
        const struct in6_addr& addr,
        const in_port_t        port);
    /**
     * Constructs from an IPv4 socket address.
     * @param[in] addr  IPv4 socket address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddrImpl(const struct sockaddr_in& addr);
    /**
     * Constructs from an IPv6 socket address.
     * @param[in] addr  IPv6 socket address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddrImpl(const struct sockaddr_in6& sockaddr);
    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     * @exceptionsafety Nothrow
     */
    size_t hash() const noexcept;
    /**
     * Compares this instance with another.
     * @param[in] that  Other instance
     * @retval <0  This instance is less than the other
     * @retval  0  This instance is equal to the other
     * @retval >0  This instance is greater than the other
     * @exceptionsafety Nothrow
     */
    int compare(const InetSockAddrImpl& that) const noexcept;
    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @retval `true`   This instance equals the other
     * @retval `false`  This instance doesn't equal the other
     * @exceptionsafety Nothrow
     */
    bool equals(const InetSockAddrImpl& that) const noexcept {
        return compare(that) == 0;
    }
    /**
     * Returns the string representation of the Internet socket address.
     * @return String representation of the Internet socket address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const;
    /**
     * Connects a socket to this instance's endpoint.
     * @param[in] sd  Socket descriptor
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void connect(int sd) const {
        inetAddr.connect(sd, port);
    }
    /**
     * Binds this instance's endpoint to a socket.
     * @param[in] sd  Socket descriptor
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void bind(int sd) const {
        inetAddr.bind(sd, port);
    }
};

} // namespace

#endif /* INETSOCKADDRIMPL_H_ */
