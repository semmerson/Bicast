/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddr.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an Internet address.
 */

#include "Inet4Addr.h"
#include "Inet6Addr.h"
#include "InetAddr.h"
#include "InetAddrImpl.h"

namespace hycast {

/**
 * Constructs from nothing.
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
InetAddr::InetAddr()
    : pImpl{InetAddrImpl::create()}
{
}

/**
 * Constructs from a string representation of an Internet address.
 * @param[in] ip_addr  String representation of an Internet address
 * @throws std::bad_alloc if required memory can't be allocated
 * @throws std::invalid_argument if the string representation is invalid
 * @exceptionsafety Strong
 */
InetAddr::InetAddr(const std::string ip_addr)
    : pImpl(InetAddrImpl::create(ip_addr))
{
}

/**
 * Constructs from an IPv4 address.
 * @param[in] addr  IPv4 address
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
InetAddr::InetAddr(const in_addr_t addr)
    : pImpl{new Inet4Addr(addr)}
{
}

/**
 * Constructs from a of an IPv4 address.
 * @param[in] addr  An IPv4 address
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
InetAddr::InetAddr(const struct in_addr& addr)
    : pImpl{new Inet4Addr(addr)}
{
}

/**
 * Constructs from an IPv6 address.
 * @param[in] addr  An IPv6 address
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
InetAddr::InetAddr(const struct in6_addr& addr)
    : pImpl{new Inet6Addr(addr)}
{
}

/**
 * Constructs from another instance.
 * @param[in] that  Other instance
 * @exceptionsafety Nothrow
 */
InetAddr::InetAddr(const InetAddr& that) noexcept
    : pImpl(that.pImpl)
{
}

/**
 * Returns the address family of this instance.
 * @retval AF_INET   IPv4 family
 * @retval AF_INET6  IPv6 family
 * @exceptionsafety Nothrow
 */
int InetAddr::get_family() const noexcept
{
    return pImpl->get_family();
}

/**
 * Compares this instance with another.
 * @param[in] that  Other instance
 * @retval <0  This instance is less than the other
 * @retval  0  This instance is equal to the other
 * @retval >0  This instance is greater than the other
 * @exceptionsafety Nothrow
 */
int InetAddr::compare(const InetAddr& that) const noexcept
{
    return pImpl->compare(*that.pImpl.get());
}

/**
 * Assigns this instance from another.
 * @param[in] rhs  Other instance
 * @return This instance
 * @exceptionsafety Nothrow
 */
InetAddr& InetAddr::operator=(const InetAddr& rhs) noexcept
{
    pImpl = rhs.pImpl; // InetAddrImpl class is immutable
    return *this;
}

/**
 * Returns the hash code of this instance.
 * @return hash code of this instance
 * @exceptionsafety Nothrow
 */
size_t InetAddr::hash() const noexcept
{
    return pImpl->hash();
}

/**
 * Returns the string representation of the Internet address.
 * @return The string representation of the Internet address
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
std::string InetAddr::to_string() const
{
    return pImpl->to_string();
}

void InetAddr::connect(
        int       sd,
        in_port_t port) const
{
    pImpl->connect(sd, port);
}

void InetAddr::bind(
        int       sd,
        in_port_t port) const
{
    pImpl->bind(sd, port);
}

} // namespace
