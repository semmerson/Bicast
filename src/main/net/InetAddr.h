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

class InetAddr final {
    std::shared_ptr<InetAddrImpl> pImpl;
    /**
     * Constructs from a shared pointer to an implementation.
     * @param[in] pImpl  Shared pointer to implementation
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
    explicit InetAddr(const std::string addr = "localhost")
        : pImpl{create(addr).pImpl} {}
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
     * Gets the socket addresses corresponding to a port number.
     * @param[in]  port Port number
     * @return     Set of socket addresses
     * @throws std::system_error if the IP address couldn't be obtained
     * @throws std::system_error if required memory couldn't be allocated
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    std::shared_ptr<std::set<struct sockaddr_storage>> getSockAddr(
            const in_port_t  port) const;
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
