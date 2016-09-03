/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetSockAddr.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an Internet socket address.
 */

#include "InetSockAddr.h"
#include "InetSockAddrImpl.h"

#include <netinet/in.h>

namespace hycast {

/**
 * Constructs from nothing. The resulting object will be the default IPv4 socket
 * address.
 * @throws std::bad_alloc if necessary memory can't be allocated
 * @exceptionsafety Strong
 */
InetSockAddr::InetSockAddr()
    : pImpl{new InetSockAddrImpl()}
{
}

/**
 * Constructs from a string representation of an IP address and a port number.
 * @param[in] ip_addr  IP address (either IPv4 or IPv6)
 * @param[in] port     Port number
 * @throws std::invalid_argument if the IP address is invalid
 * @throws std::bad_alloc if necessary memory can't be allocated
 * @exceptionsafety Strong
 */
InetSockAddr::InetSockAddr(
        const std::string ip_addr,
        const in_port_t   port)
    : pImpl(new InetSockAddrImpl(ip_addr, port))
{
}

/**
 * Constructs from an IPv4 address.
 * @param[in] addr  IPv4 address in _network_ byte order
 * @param[in] port  Port number in _host_ bytes order
 * @throws std::bad_alloc if necessary memory can't be allocated
 * @exceptionsafety Strong
 */
InetSockAddr::InetSockAddr(
        const in_addr_t  addr,
        const PortNumber port)
    : pImpl{new InetSockAddrImpl(addr, port)}
{
}

/**
 * Constructs from an IPv6 address.
 * @param[in] addr  IPv6 address in _network_ byte order
 * @param[in] port  Port number in _host_ bytes order
 * @throws std::bad_alloc if necessary memory can't be allocated
 * @exceptionsafety Strong
 */
InetSockAddr::InetSockAddr(
        const struct in6_addr& addr,
        const in_port_t        port)
    : pImpl{new InetSockAddrImpl(addr, port)}
{
}

/**
 * Constructs from an IPv4 socket address.
 * @param[in] addr  IPv4 socket address
 * @throws std::bad_alloc if necessary memory can't be allocated
 * @exceptionsafety Strong
 */
InetSockAddr::InetSockAddr(const struct sockaddr_in& addr)
    : pImpl{new InetSockAddrImpl(addr)}
{
}

/**
 * Constructs from an IPv6 socket address.
 * @param[in] addr  IPv6 socket address
 * @throws std::bad_alloc if necessary memory can't be allocated
 * @exceptionsafety Strong
 */
InetSockAddr::InetSockAddr(const struct sockaddr_in6& sockaddr)
    : pImpl{new InetSockAddrImpl(sockaddr)}
{
}

/**
 * Copy constructs from another instance.
 * @param[in] that  Other instance
 * @exceptionsafety Nothrow
 */
InetSockAddr::InetSockAddr(const InetSockAddr& that) noexcept
    : pImpl(that.pImpl)
{
}

/**
 * Copy assigns from an instance.
 * @param[in] rhs  An instance
 * @exceptionsafety Nothrow
 */
InetSockAddr& InetSockAddr::operator =(const InetSockAddr& rhs) noexcept
{
    pImpl = rhs.pImpl; // InetSockAddrImpl is an immutable class
    return *this;
}

/**
 * Returns the hash code of this instance.
 * @return This instance's hash code
 * @exceptionsafety Nothrow
 */
size_t InetSockAddr::hash() const noexcept
{
    return pImpl->hash();
}

/**
 * Returns a string representation of this instance.
 * @return A string representation of this instance
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
std::string InetSockAddr::to_string() const
{
    return pImpl->to_string();
}

/**
 * Compares this instance with another.
 * @param[in] that  Other instance
 * @retval <0 This instance is less than the other
 * @retval  0 This instance is eaual to the other
 * @retval >0 This instance is greater than the other
 * @exceptionsafety Nothrow
 */
int InetSockAddr::compare(const InetSockAddr& that) const noexcept
{
    return pImpl->compare(*that.pImpl);
}

} // namespace
