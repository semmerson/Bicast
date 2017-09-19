/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Ipv6Addr.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an IPv6 address.
 */

#include "config.h"

#include "error.h"
#include "Ipv6Addr.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

void Ipv6Addr::setSockAddrStorage(
        sockaddr_storage& storage,
        const int         port,
        const int         sockType) const noexcept
{
    ::memset(&storage, 0, sizeof(storage));
    struct sockaddr_in6* sockAddr =
            reinterpret_cast<struct sockaddr_in6*>(&storage);
    sockAddr->sin6_family = AF_INET6;
    sockAddr->sin6_port = htons(port);
    sockAddr->sin6_addr = ipAddr;
}

size_t Ipv6Addr::hash() const noexcept
{
    const size_t*       ptr = reinterpret_cast<const size_t*>(&ipAddr.s6_addr);
    const size_t* const out = ptr + sizeof(ipAddr.s6_addr)/sizeof(size_t);
    size_t              hash = 0;
    while (ptr < out)
        hash ^= *ptr++;
    return hash;
}

std::string Ipv6Addr::to_string() const
{
    char buf[INET6_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET6, &ipAddr.s6_addr, buf, sizeof(buf)));
}

int Ipv6Addr::getSocket(const int sockType) const
{
    int sd = ::socket(AF_INET6, sockType, 0);
    if (sd == -1)
        throw std::system_error(errno, std::system_category(),
                "socket() failure: sockType=" + std::to_string(sockType));
    return sd;
}

unsigned Ipv6Addr::getIfaceIndex(const int sd) const
{
    int lastlen = 0;
    for (size_t size = 1024; ; size *= 2) {
        char          buf[size];
        struct ifconf ifc;
        if (ioctl(sd, SIOCGIFCONF, &ifc) < 0) {
            if (errno != EINVAL || lastlen != 0)
                throw SYSTEM_ERROR("Couldn't get interface configurations");
        }
        else if (ifc.ifc_len != lastlen) {
            lastlen = ifc.ifc_len;
        }
        else {
            int                 len;
            const struct ifreq* ifr;
            for (const char* ptr = buf; ptr < buf + ifc.ifc_len;
                    ptr += sizeof(ifr->ifr_name) + len) {
                ifr = reinterpret_cast<const struct ifreq*>(ptr);
#ifdef HAVE_STRUCT_IFREQ_IF_ADDR_SA_LEN
                len = std::max(sizeof(struct sockaddr),
                        ifr->ifr_addr.sa_len);
#else
                switch (ifr->ifr_addr.sa_family) {
                    case AF_INET6:
                        len = sizeof(struct sockaddr_in6);
                    break;
                    case AF_INET:
                    default:
                        len = sizeof(struct sockaddr);
                }
#endif
                if (ifr->ifr_addr.sa_family != AF_INET6)
                    continue;
                auto sockAddr = reinterpret_cast<const struct sockaddr_in6*>
                        (&ifr->ifr_addr);
                if (::memcmp(&sockAddr->sin6_addr, &ipAddr, sizeof(ipAddr))
                        == 0) {
                    unsigned index = if_nametoindex(ifr->ifr_name);
                    if (index == 0)
                        throw SYSTEM_ERROR("Couldn't convert interface name \""
                                + std::string(ifr->ifr_name) + "\" to index");
                    return index;
                } // Found matching entry
            } // Interface entry loop
        } // Have interface entries
    } // Interface entries buffer-size increment loop
    throw NOT_FOUND_ERROR("Couldn't find interface entry corresponding to " +
            to_string());
}

void Ipv6Addr::setInterface(const int sd) const
{
    unsigned ifaceIndex = getIfaceIndex(sd);
    if (setsockopt(sd, IPPROTO_IP, IPV6_MULTICAST_IF, &ifaceIndex,
            sizeof(ifaceIndex)))
        throw SYSTEM_ERROR("Couldn't set output interface to " + to_string());
}

void Ipv6Addr::setHopLimit(
        const int      sd,
        const unsigned limit) const
{
    if (limit > 255)
        throw std::invalid_argument("Invalid hop-limit: " +
                std::to_string(limit));
    const int value = limit;
    if (::setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &value, sizeof(value)))
        throw std::system_error(errno, std::system_category(),
                std::string("Couldn't set hop-limit for multicast packets: "
                "sock=") + std::to_string(sd) + ", group=" + to_string() +
                ", limit=" + std::to_string(value));
}

void Ipv6Addr::setMcastLoop(
        const int  sd,
        const bool enable) const
{
    const unsigned value = enable;
    if (::setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &value,
            sizeof(value)))
        throw std::system_error(errno, std::system_category(),
                std::string("Couldn't set multicast packet looping: "
                "sock=") + std::to_string(sd) + ", enable=" +
                std::to_string(enable));
}

} // namespace
