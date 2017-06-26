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

#include "InetNameAddr.h"
#include "Ipv4Addr.h"
#include "Ipv6Addr.h"

#include <cstring>
#include <errno.h>
#include <IpAddrImpl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

InetNameAddr::InetNameAddr(const std::string name)
    : name{name}
{}

void InetNameAddr::setSockAddrStorage(
        sockaddr_storage& storage,
        const int         port,
        const int         sockType) const noexcept
{
    IpAddrImpl* const ipAddr = getIpAddr();
    try {
        ipAddr->setSockAddrStorage(storage, port, sockType);
        delete ipAddr;
    }
    catch (...) {
        delete ipAddr;
        throw;
    }
}

std::string InetNameAddr::to_string() const noexcept
{
    return name;
}

void InetNameAddr::getSockAddrs(
        const int                                family,
        const int                                sockType,
        const in_port_t                          port,
        std::set<struct sockaddr_storage>* const set) const
{
    struct addrinfo  hints = {};
    hints.ai_family = family;
    hints.ai_socktype = sockType;
    hints.ai_protocol = (sockType == SOCK_STREAM)
            ? IPPROTO_SCTP
            : sockType == SOCK_DGRAM
              ? IPPROTO_UDP
              : 0;
    struct addrinfo* list;
    if (::getaddrinfo(name.data(), nullptr, &hints, &list))
        throw std::system_error(errno, std::system_category(),
                std::string("::getaddrinfo() failure for host \"") +
                name.data() + "\"");
    try {
        uint16_t netPort = htons(port);
        for (struct addrinfo* entry = list; entry != NULL;
                entry = entry->ai_next) {
            union {
                struct sockaddr_in      ipv4;
                struct sockaddr_in6     ipv6;
                struct sockaddr_storage storage;
            } sockaddr = {};
            if (entry->ai_addr->sa_family == AF_INET) {
                sockaddr.ipv4 = *reinterpret_cast<
                        struct sockaddr_in*>(entry->ai_addr);
                sockaddr.ipv4.sin_port = netPort;
                set->insert(sockaddr.storage);
            }
            else if (entry->ai_addr->sa_family == AF_INET6) {
                sockaddr.ipv6 = *reinterpret_cast<
                        struct sockaddr_in6*>(entry->ai_addr);
                sockaddr.ipv6.sin6_port = netPort;
                set->insert(sockaddr.storage);
            }
        }
        ::freeaddrinfo(list);
    }
    catch (const std::exception& e) {
        ::freeaddrinfo(list);
        throw;
    }
}

IpAddrImpl* InetNameAddr::getIpAddr(const int family) const
{
    struct addrinfo  hints = {};
    hints.ai_family = family; // Use first entry
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo* list;
    if (::getaddrinfo(name.data(), nullptr, &hints, &list))
        throw std::system_error(errno, std::system_category(),
                std::string("::getaddrinfo() failure for host \"") +
                name.data() + "\"");
    try {
        IpAddrImpl* ipAddr = nullptr;
        for (struct addrinfo* entry = list; entry != NULL;
                entry = entry->ai_next) {
            if (entry->ai_addr->sa_family == AF_INET) {
                struct sockaddr_in* sockAddr = reinterpret_cast<
                        struct sockaddr_in*>(entry->ai_addr);
                ipAddr = new Ipv4Addr(sockAddr->sin_addr.s_addr);
                break;
            }
            else if (entry->ai_addr->sa_family == AF_INET6) {
                struct in6_addr addr = *reinterpret_cast<
                        struct in6_addr*>(entry->ai_addr->sa_data);
                ipAddr = new Ipv6Addr(addr);
                break;
            }
        }
        freeaddrinfo(list);
        return ipAddr;
    }
    catch (...) {
        freeaddrinfo(list);
        throw;
    }
}

IpAddrImpl* InetNameAddr::getIpAddr() const
{
    IpAddrImpl* ipAddr = getIpAddr(AF_INET);
    return (ipAddr != nullptr)
            ? ipAddr
            : getIpAddr(AF_INET6);
}

int InetNameAddr::getSocket(const int sockType) const
{
    IpAddrImpl* const ipAddr = getIpAddr();
    try {
        int sd = ipAddr->getSocket(sockType);
        delete ipAddr;
        return sd;
    }
    catch (...) {
        delete ipAddr;
        throw;
    }
}

void InetNameAddr::setInterface(const int sd) const
{
    IpAddrImpl* const ipAddr = getIpAddr();
    try {
        ipAddr->setInterface(sd);
        delete ipAddr;
    }
    catch (...) {
        delete ipAddr;
        throw;
    }
}

void InetNameAddr::setHopLimit(
        const int      sd,
        const unsigned limit) const
{
    IpAddrImpl* const ipAddr = getIpAddr();
    try {
        ipAddr->setHopLimit(sd, limit);
        delete ipAddr;
    }
    catch (...) {
        delete ipAddr;
        throw;
    }
}

void InetNameAddr::setMcastLoop(
        const int  sd,
        const bool enable) const
{
    IpAddrImpl* const ipAddr = getIpAddr();
    try {
        ipAddr->setMcastLoop(sd, enable);
        delete ipAddr;
    }
    catch (...) {
        delete ipAddr;
        throw;
    }
}

} // namespace
