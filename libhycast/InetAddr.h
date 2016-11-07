/**
 * This file declares an immutable Internet address.
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
#include <string>

namespace hycast {

class InetAddrImpl; // Forward declaration of implementation

class InetAddr final {
    std::shared_ptr<InetAddrImpl> pImpl;
    /**
     * Constructs from a shared pointer to an implementation.
     * @param[in] pImpl  Shared pointer to implementation
     */
    InetAddr(std::shared_ptr<InetAddrImpl> pImpl)
        : pImpl{pImpl} {}
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
    explicit InetAddr(const std::string addr = "localhost")
        : pImpl{create(addr).pImpl} {}
    /**
     * Returns the string representation of the Internet address.
     * @return The string representation of the Internet address
     * @exceptionsafety Strong
     */
    std::string to_string() const;
    /**
     * Gets the socket address corresponding to a port number.
     * @param[in]  port      Port number
     * @param[out] sockAddr  Resulting socket address
     * @param[out] sockLen   Size of socket address in bytes
     * @throws std::system_error if the IP address couldn't be obtained
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void getSockAddr(
            const in_port_t  port,
            struct sockaddr& sockAddr,
            socklen_t&       sockLen) const;
    /**
     * Connects a socket to the given port of this instance's endpoint.
     * @param[in] sd    Socket descriptor
     * @param[in] port  Port number in host byte order
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void connect(
            int       sd,
            in_port_t port) const;
    /**
     * Binds a socket to the given port of this instance's endpoint.
     * @param[in] sd    Socket descriptor
     * @param[in] port  Port number in host byte order
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void bind(
            int       sd,
            in_port_t port) const;
};

} // namespace

#endif /* INETADDR_H_ */
