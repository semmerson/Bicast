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
#include <set>

namespace hycast {

class InetNameAddr final : public InetAddrImpl {
    std::string name;
    /**
     * Adds Internet SCTP address to a set.
     * @param[in]  family  Internet address family of addresses to add
     * @param[in]  port    Port number in host byte order
     * @param[in,out] set  Set of Internet addresses.
     * @throws std::system_error if the IP address couldn't be obtained
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void getSockAddrs(
            const int                        family,
            const in_port_t                  port,
            std::set<struct sockaddr>* const set) const;
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
     * Gets the socket addresses corresponding to a port number.
     * @param[in]  port Port number in host byte order
     * @return     Set of socket addresses
     * @throws std::system_error if the IP address couldn't be obtained
     * @throws std::system_error if required memory couldn't be allocated
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    virtual std::shared_ptr<std::set<struct sockaddr>> getSockAddr(
            const in_port_t  port) const;
};

} // namespace

#endif /* INETNAMEADDR_H_ */
