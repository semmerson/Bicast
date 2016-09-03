/**
 * This file declares an immutable IPv4 address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Inet4Addr.h
 * @author: Steven R. Emmerson
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
    /**
     * Default construction. The IPv4 address will correspond to "0.0.0.0"
     * (INADDR_ANY).
     * @exceptionsafety Nothrow
     */
    Inet4Addr() noexcept : addr{0} {}
    /**
     * Constructs from a string representation of an IPv4 address.
     * @param[in] ipAddr  A string representation of an IPv4 address
     * @throws std::invalid_argument if the string represents an invalid IPv4
     *                               address
     * @exceptionsafety Strong
     */
    explicit Inet4Addr(const std::string ipAddr);
    /**
     * Constructs from an IPv4 address.
     * @param[in] ipAddr  IPv4 address
     * @exceptionsafety Nothrow
     */
    explicit Inet4Addr(const in_addr_t ipAddr) noexcept : addr(ipAddr) {};
    /**
     * Constructs from an IPv4 address.
     * @param[in] ipAddr  IPv4 address
     * @exceptionsafety Nothrow
     */
    explicit Inet4Addr(const struct in_addr ipAddr) noexcept : addr{ipAddr.s_addr} {};
    /**
     * Returns the address family.
     * @retval AF_INET
     * @exceptionsafety Nothrow
     */
    int get_family() const noexcept {return AF_INET;}
    /**
     * Returns the hash code for this instance.
     * @return Hash code for this instance
     * @exceptionsafety Nothrow
     */
    size_t hash() const noexcept {return std::hash<in_addr_t>()(addr);}
    /**
     * Compares this instance with another.
     * @param[in] that  Other instance
     * @retval <0  This instance is less than the other
     * @retval  0  This instance is equal to the other
     * @retval >0  This instance is greater than the other
     * @exceptionsafety Nothrow
     */
    int compare(const Inet4Addr& that) const noexcept;
    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  Other instance
     * @retval `true`   This instance is equal to the other
     * @retval `false`  This instance isn't equal to the other
     * @exceptionsafety Nothrow
     */
    int equals(const Inet4Addr& that) const noexcept {return compare(that) == 0;}
    /**
     * Returns the string representation of the IPv4 address.
     * @return The string representation of the IPv4 address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const;
};

} // namespace

#endif /* INET4ADDR_H_ */
