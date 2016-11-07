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
#include "IpAddr.h"

#include <cstddef>
#include <memory>
#include <netinet/in.h>

namespace hycast {

class InetNameAddr final : public InetAddrImpl {
    std::string name;
    /**
     * Returns a corresponding IP address.
     * @return a corresponding IP address
     * @throws std::system_error if the IP address couldn't be obtained
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    std::shared_ptr<IpAddr> getIpAddr() const;
public:
    /**
     * Constructs from a hostname.
     * @param[in] name  A hostname
     */
    explicit InetNameAddr(const std::string name);
    /**
     * Returns the hostname.
     * @return The hostname
     * @exceptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    std::string to_string() const noexcept;
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
     * @param[in] port  Port number
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

#endif /* INETNAMEADDR_H_ */
