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

#include <cstddef>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <set>
#include <sys/socket.h>

namespace hycast {

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
     * Destructor.
     */
    virtual             ~InetAddrImpl() {};
    /**
     * Returns the string representation of the Internet address.
     * @return The string representation of the Internet address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    virtual std::string to_string() const = 0;
    /**
     * Gets the socket addresses corresponding to a port number.
     * @param[in]  port Port number
     * @return     Set of socket addresses
     * @throws std::system_error if the IP address couldn't be obtained
     * @throws std::system_error if required memory couldn't be allocated
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    virtual std::shared_ptr<std::set<struct sockaddr>> getSockAddr(
            const in_port_t  port) const =0;
};

} // namespace

namespace std {
    template<>
    struct less<struct sockaddr> {
        bool operator()(
            const struct sockaddr sockaddr1,
            const struct sockaddr sockaddr2)
        {
            return ::memcmp(sockaddr1.sa_data, sockaddr2.sa_data,
                    sizeof(sockaddr1)) < 0;
        }
    };
}

#endif /* INETADDRIMPL_H_ */
