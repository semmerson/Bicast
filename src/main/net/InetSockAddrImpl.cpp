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
#include <system_error>

namespace hycast {

InetSockAddrImpl::InetSockAddrImpl()
    : inetAddr(),
      port(0)
{}

InetSockAddrImpl::InetSockAddrImpl(
        const std::string ipAddr,
        const in_port_t   port)
    : inetAddr(ipAddr),
      port(port)
{}

InetSockAddrImpl::InetSockAddrImpl(
        const in_addr_t  addr,
        const PortNumber port)
    : inetAddr{addr},
      port{port.get_host()}
{}

InetSockAddrImpl::InetSockAddrImpl(
        const struct in6_addr& addr,
        const in_port_t        port)
    : inetAddr(addr),
      port(port)
{}

InetSockAddrImpl::InetSockAddrImpl(const struct sockaddr_in& addr)
    : inetAddr(addr.sin_addr.s_addr),
      port(ntohs(addr.sin_port))
{}

InetSockAddrImpl::InetSockAddrImpl(const struct sockaddr_in6& sockaddr)
    : inetAddr(sockaddr.sin6_addr),
      port(ntohs(sockaddr.sin6_port))
{}

std::string InetSockAddrImpl::to_string() const
{
    std::string addr = inetAddr.to_string();
    return (addr.find(':') == std::string::npos)
            ? addr + ":" + std::to_string(port)
            : std::string("[") + addr + "]:" + std::to_string(port);
}

void InetSockAddrImpl::connect(const int sd) const
{
    struct sockaddr sockAddr;
    socklen_t       sockLen;
    inetAddr.getSockAddr(port, sockAddr, sockLen);
    int status = ::connect(sd, &sockAddr, sockLen);
    if (status)
        throw std::system_error(errno, std::system_category(),
                "connect() failure: socket=" + std::to_string(sd) +
                ", addr=" + to_string());
}

void InetSockAddrImpl::bind(int sd) const
{
    struct sockaddr sockAddr;
    socklen_t       sockLen;
    inetAddr.getSockAddr(port, sockAddr, sockLen);
    int status = ::bind(sd, &sockAddr, sockLen);
    if (status)
        throw std::system_error(errno, std::system_category(),
                "bind() failure: socket=" + std::to_string(sd) +
                ", addr=" + to_string());
}

} // namespace
