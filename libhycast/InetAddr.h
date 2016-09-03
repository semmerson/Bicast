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

class InetAddrImpl; // Forward declaration

class InetAddr final {
    std::shared_ptr<InetAddrImpl> pImpl;
public:
    /**
     * Constructs from nothing.
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    InetAddr();
    /**
     * Constructs from a string representation of an Internet address.
     * @param[in] ip_addr  String representation of an Internet address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    explicit InetAddr(const std::string ip_addr);
    /**
     * Constructs from an IPv4 address.
     * @param[in] addr  An IPv4 address
     * @exceptionsafety Strong
     */
    explicit InetAddr(const in_addr_t  addr);
    /**
     * Constructs from a of an IPv4 address.
     * @param[in] addr  An IPv4 address
     * @exceptionsafety Strong
     */
    explicit InetAddr(const struct in_addr& addr);
    /**
     * Constructs from an IPv6 address.
     * @param[in] addr  An IPv6 address
     * @exceptionsafety Strong
     */
    explicit InetAddr(const struct in6_addr& addr);
    /**
     * Constructs from another instance.
     * @param[in] that  Other instance
     * @exceptionsafety Nothrow
     */
    InetAddr(const InetAddr& that) noexcept;
    /**
     * Returns the address family of this instance.
     * @retval AF_INET   IPv4 family
     * @retval AF_INET6  IPv6 family
     * @exceptionsafety Nothrow
     */
    int get_family() const noexcept;
    /**
     * Assigns this instance from another.
     * @param[in] rhs  Other instance
     * @return This instance
     * @exceptionsafety Nothrow
     */
    InetAddr& operator=(const InetAddr& rhs) noexcept;
    /**
     * Returns the hash code of this instance.
     * @return hash code of this instance
     * @exceptionsafety Nothrow
     */
    size_t hash() const noexcept;
    /**
     * Compares this instance with another.
     * @param[in] that  Other instance
     * @retval <0  This instance is less than the other
     * @retval  0  This instance is equal to the other
     * @retval >0  This instance is greater than the other
     * @exceptionsafety Nothrow
     */
    int compare(const InetAddr& that) const noexcept;
    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  Other instance
     * @retval `true`   This instance is equal to the other
     * @retval `false`  This instance isn't equal to the other
     * @exceptionsafety Nothrow
     */
    bool equals(const InetAddr& that) const noexcept {return compare(that) == 0;}
    /**
     * Returns the string representation of the Internet address.
     * @return The string representation of the Internet address
     * @exceptionsafety Strong
     */
    std::string to_string() const;
};

} // namespace

#endif /* INETADDR_H_ */
