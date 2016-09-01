/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Inet4Addr.h
 * @author: Steven R. Emmerson
 *
 * This file declares an IPv4 address
 */

#ifndef INET4ADDR_H_
#define INET4ADDR_H_

#include "InetAddrImpl.h"

#include <cstddef>
#include <functional>
#include <netinet/in.h>

namespace hycast {

class Inet4Addr final : public InetAddrImpl {
    in_addr_t addr;
public:
    Inet4Addr() : addr{0} {}
    explicit Inet4Addr(const std::string ipAddr);
    explicit Inet4Addr(const in_addr_t ipAddr) : addr(ipAddr) {};
    explicit Inet4Addr(const struct in_addr ipAddr) : addr{ipAddr.s_addr} {};
    int get_family() const {return AF_INET;}
    size_t hash() const {return std::hash<in_addr_t>()(addr);}
    int compare(const Inet4Addr& that) const;
    int equals(const Inet4Addr& that) const {return compare(that) == 0;}
    std::string to_string() const;
};

} // namespace

#endif /* INET4ADDR_H_ */
