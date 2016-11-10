/**
 * This file implements an Internet hostname.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetNameAddr.cpp
 * @author: Steven R. Emmerson
 */

#include "Inet4Addr.h"
#include "Inet6Addr.h"
#include "InetNameAddr.h"

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

InetNameAddr::InetNameAddr(const std::string name)
    : name{name}
{}

std::string InetNameAddr::to_string() const noexcept
{
    return name;
}

std::shared_ptr<IpAddr> InetNameAddr::getIpAddr() const
{
    IpAddr* addr;
    struct addrinfo* entry;
    if (::getaddrinfo(name.data(), nullptr, nullptr, &entry))
        throw std::system_error(errno, std::system_category(),
                std::string("Couldn't get IP address of host \"") + name.data()
                + "\"");
    try {
        if (entry->ai_family == AF_INET) {
            addr = new Inet4Addr(
                    reinterpret_cast<struct sockaddr_in*>(entry->ai_addr)->sin_addr);
        }
        else if (entry->ai_family == AF_INET6) {
            addr = new Inet6Addr(
                    reinterpret_cast<struct sockaddr_in6*>(entry->ai_addr)->sin6_addr);
        }
        else {
            throw std::system_error(errno, std::system_category(),
                    std::string("Host \"") + name.data() +
                    "\" doesn't support Internet Protocol");
        }
    }
    catch (...) {
        freeaddrinfo(entry);
        throw;
    }
    return std::shared_ptr<IpAddr>(addr);
}

void InetNameAddr::getSockAddr(
            const in_port_t  port,
            struct sockaddr& sockAddr,
            socklen_t&       sockLen) const
{
    std::shared_ptr<IpAddr> addr{getIpAddr()};
    addr->getSockAddr(port, sockAddr, sockLen);
}

} // namespace
