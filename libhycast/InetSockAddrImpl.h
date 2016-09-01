/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetSockAddrImpl.h
 * @author: Steven R. Emmerson
 *
 * This file defines an abstract base class for an Internet socket address.
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
    InetSockAddrImpl();
    InetSockAddrImpl(
            const std::string ipAddr,
            const in_port_t   port);
    InetSockAddrImpl(
            const in_addr_t  addr,
            const PortNumber port);
    InetSockAddrImpl(
        const struct in6_addr& addr,
        const in_port_t        port);
    InetSockAddrImpl(const struct sockaddr_in& addr);
    InetSockAddrImpl(const struct sockaddr_in6& sockaddr);
    size_t hash() const;
    int compare(const InetSockAddrImpl& that) const;
    bool equals(const InetSockAddrImpl& that) const {
        return compare(that) == 0;
    }
    std::string to_string() const;
};

} // namespace

#endif /* INETSOCKADDRIMPL_H_ */
