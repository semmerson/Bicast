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
#include <sys/socket.h>

namespace hycast {

class InetSockAddrImpl final {
    InetAddr  inetAddr;
    in_port_t port;  // In host byte-order
public:
    /**
     * Constructs from nothing. The resulting instance will have the default
     * Internet address and the port number will be 0.
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl();
    /**
     * Constructs from a string representation of an Internet address and a port
     * number.
     * @param[in] ipAddr  String representation of Internet address
     * @param[in] port    Port number in host byte-order
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if the string representation is invalid
     */
    InetSockAddrImpl(
            const std::string ipAddr,
            const in_port_t   port);
    /**
     * Constructs from an IPV4 address and a port number.
     * @param[in] addr  IPv4 address
     * @param[in] port  Port number
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl(
            const in_addr_t  addr,
            const PortNumber port);
    /**
     * Constructs from an IPV6 address and a port number.
     * @param[in] addr  IPv6 address
     * @param[in] port  Port number in host byte-order
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl(
        const struct in6_addr& addr,
        const in_port_t        port);
    /**
     * Constructs from an IPv4 socket address.
     * @param[in] addr  IPv4 socket address
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl(const struct sockaddr_in& addr);
    /**
     * Constructs from an IPv6 socket address.
     * @param[in] addr  IPv6 socket address
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl(const struct sockaddr_in6& sockaddr);
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
    void connect(const int sd) const;
    /**
     * Binds this instance's endpoint to a socket.
     * @param[in] sd  Socket descriptor
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void bind(int sd) const;
};

} // namespace

#endif /* INETSOCKADDRIMPL_H_ */
