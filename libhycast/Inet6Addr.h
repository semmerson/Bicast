/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Inet6Addr.h
 * @author: Steven R. Emmerson
 *
 * This file declares an immutable IPv6 address
 */

#ifndef INET6ADDR_H_
#define INET6ADDR_H_

#include "InetAddrImpl.h"

#include <cstddef>
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>

namespace hycast {

class Inet6Addr final : public InetAddrImpl {
    struct in6_addr addr;
public:
    Inet6Addr();
    explicit Inet6Addr(const std::string ipAddr);
    explicit Inet6Addr(const struct in6_addr& ipAddr);
    int get_family() const {return AF_INET6;}
    size_t hash() const;
    int compare(const Inet6Addr& that) const;
    int equals(const Inet6Addr& that) const {return compare(that) == 0;}
    std::string to_string() const;
};

} // namespace

#endif /* INET6ADDR_H_ */
