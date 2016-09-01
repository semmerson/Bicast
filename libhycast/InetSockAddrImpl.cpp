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

InetSockAddrImpl::InetSockAddrImpl()
    : inetAddr(),
      port(0)
{
}

InetSockAddrImpl::InetSockAddrImpl(
        const std::string ipAddr,
        const in_port_t   port)
    : inetAddr(ipAddr),
      port(port)
{
}

InetSockAddrImpl::InetSockAddrImpl(
        const in_addr_t  addr,
        const PortNumber port)
    : inetAddr(addr),
      port(port.get_host())
{
}

InetSockAddrImpl::InetSockAddrImpl(
        const struct in6_addr& addr,
        const in_port_t        port)
    : inetAddr(addr),
      port(port)
{
}

InetSockAddrImpl::InetSockAddrImpl(const struct sockaddr_in& addr)
    : inetAddr(addr.sin_addr),
      port(ntohs(addr.sin_port))
{
}

InetSockAddrImpl::InetSockAddrImpl(const struct sockaddr_in6& sockaddr)
    : inetAddr(sockaddr.sin6_addr),
      port(ntohs(sockaddr.sin6_port))
{
}

size_t InetSockAddrImpl::hash() const
{
    return inetAddr.hash() ^ std::hash<in_port_t>()(port);
}

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

std::string InetSockAddrImpl::to_string() const
{
    return (inetAddr.get_family() == AF_INET)
            ? inetAddr.to_string() + ":" + std::to_string(port)
            : std::string("[") + inetAddr.to_string() + "]:" + std::to_string(port);
}

} // namespace
