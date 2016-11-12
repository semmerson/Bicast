/**
 * This file declares an immutable Internet socket address.
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
     * Internet address and port number 0.
     * @throws std::bad_alloc if necessary memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddr();
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
     * Copy constructs from another instance.
     * @param[in] that  Other instance
     * @exceptionsafety Nothrow
     */
    InetSockAddr(const InetSockAddr& that) noexcept;
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
     * Compares this instance with another.
     * @param[in] that  Other instance
     * @retval <0 This instance is less than the other
     * @retval  0 This instance is equal to the other
     * @retval >0 This instance is greater than the other
     * @exceptionsafety Nothrow
     */
    int compare(const InetSockAddr& that) const noexcept;
    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @retval <0  This instance is less than the other
     * @retval  0  This instance is equal to the other
     * @retval >0  This instance is greater than the other
     * @exceptionsafety Nothrow
     */
    bool operator==(const InetSockAddr& that) const noexcept {
        return compare(that) == 0;
    }
    /**
     * Returns a string representation of this instance.
     * @return A string representation of this instance
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
    void connect(int sd) const;
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

#endif /* INETSOCKADDR_H_ */
