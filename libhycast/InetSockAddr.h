/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetSockAddr.h
 * @author: Steven R. Emmerson
 *
 * This file declares an Internet socket address.
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
    InetSockAddr();
    InetSockAddr(
            const std::string ip_addr,
            const in_port_t   port);
    InetSockAddr(
            const in_addr_t  addr,
            const PortNumber port);
    InetSockAddr(const struct sockaddr_in& addr);
    InetSockAddr(
            const struct in6_addr& addr,
            const in_port_t        port);
    InetSockAddr(const struct sockaddr_in6& sockaddr);
    InetSockAddr(const InetSockAddr& that);
    InetSockAddr& operator=(const InetSockAddr& rhs);
    size_t hash() const;
    int compare(const InetSockAddr& that) const;
    bool equals(const InetSockAddr& that) const {
        return compare(that) == 0;
    }
    std::string to_string() const;
};

} // namespace

#endif /* INETSOCKADDR_H_ */
