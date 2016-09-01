/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetSockAddrImpl.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an Internet socket address.
 */

#include "InetSockAddrImpl.h"

#include <arpa/inet.h>
#include <functional>
#include <sys/socket.h>

namespace hycast {

/**
 * Constructs from nothing. The Internet address will be "0.0.0.0" (INET_ANY)
 * and the port number will be 0.
 */
InetSockAddrImpl::InetSockAddrImpl()
    : inetAddr(),
      port(0)
{
}

/**
 * Constructs from a string representation of an Internet address and a port
 * number.
 * @param[in] ipAddr  String representation of Internet address
 * @param[in] port    Port number in host byte-order
 */
InetSockAddrImpl::InetSockAddrImpl(
        const std::string ipAddr,
        const in_port_t   port)
    : inetAddr(ipAddr),
      port(port)
{
}

/**
 * Constructs from an IPV4 address and a port number.
 * @param[in] addr  IPv4 address
 * @param[in] port  Port number in host byte-order
 */
InetSockAddrImpl::InetSockAddrImpl(
        const in_addr_t  addr,
        const PortNumber port)
    : inetAddr(addr),
      port(port.get_host())
{
}

/**
 * Constructs from an IPV6 address and a port number.
 * @param[in] addr  IPv6 address
 * @param[in] port  Port number in host byte-order
 */
InetSockAddrImpl::InetSockAddrImpl(
        const struct in6_addr& addr,
        const in_port_t        port)
    : inetAddr(addr),
      port(port)
{
}

/**
 * Constructs from an IPv4 socket address.
 * @param[in] addr  IPv4 socket address
 */
InetSockAddrImpl::InetSockAddrImpl(const struct sockaddr_in& addr)
    : inetAddr(addr.sin_addr),
      port(ntohs(addr.sin_port))
{
}

/**
 * Constructs from an IPv6 socket address.
 * @param[in] addr  IPv6 socket address
 */
InetSockAddrImpl::InetSockAddrImpl(const struct sockaddr_in6& sockaddr)
    : inetAddr(sockaddr.sin6_addr),
      port(ntohs(sockaddr.sin6_port))
{
}

/**
 * Returns the hash code of this instance.
 * @return The hash code of this instance
 */
size_t InetSockAddrImpl::hash() const
{
    return inetAddr.hash() ^ std::hash<in_port_t>()(port);
}

/**
 * Compares this instance with another.
 * @param[in] that  Other instance
 * @retval <0  This instance is less than the other
 * @retval  0  This instance is equal to the other
 * @retval >0  This instance is greater than the other
 */
int InetSockAddrImpl::compare(const InetSockAddrImpl& that) const
{
    int cmp = inetAddr.compare(that.inetAddr);
    if (cmp)
        return cmp;
    return port < that.port
            ? -1
            : port == that.port
              ? 0
              : 1;
}

/**
 * Returns the string representation of the Internet socket address.
 * @return String representation of the Internet socket address
 */
std::string InetSockAddrImpl::to_string() const
{
    return (inetAddr.get_family() == AF_INET)
            ? inetAddr.to_string() + ":" + std::to_string(port)
            : std::string("[") + inetAddr.to_string() + "]:" + std::to_string(port);
}

} // namespace
