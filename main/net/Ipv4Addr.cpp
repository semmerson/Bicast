/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Ipv4Addr.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an IPv4 address.
 */

#include "error.h"
#include "Ipv4Addr.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

void Ipv4Addr::setSockAddrStorage(
        sockaddr_storage& storage,
        const int         port,
        const int         sockType) const noexcept
{
    ::memset(&storage, 0, sizeof(storage));
    struct sockaddr_in* sockAddr =
            reinterpret_cast<struct sockaddr_in*>(&storage);
    sockAddr->sin_family = AF_INET;
    sockAddr->sin_port = htons(port);
    sockAddr->sin_addr.s_addr = ipAddr;
}

std::string Ipv4Addr::to_string() const
{
    char buf[INET_ADDRSTRLEN];
    return std::string(inet_ntop(AF_INET, &ipAddr, buf, sizeof(buf)));
}

int Ipv4Addr::getSocket(const int sockType) const
{
    int sd = ::socket(AF_INET, sockType, 0);
    if (sd == -1)
        throw std::system_error(errno, std::system_category(),
                "socket() failure: sockType=" + std::to_string(sockType));
    return sd;
}

void Ipv4Addr::setInterface(const int sd) const
{
    if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &ipAddr, sizeof(ipAddr)))
        throw SYSTEM_ERROR("Couldn't set output interface to " + to_string());
}

void Ipv4Addr::setHopLimit(
        const int      sd,
        const unsigned limit) const
{
    if (limit > 255)
        throw std::invalid_argument("Invalid hop-limit: " +
                std::to_string(limit));
    const unsigned char value = limit;
    if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &value, sizeof(value)))
        throw std::system_error(errno, std::system_category(),
                std::string("Couldn't set hop-limit for multicast packets: "
                "sock=") + std::to_string(sd) + ", group=" + to_string() +
                ", limit=" + std::to_string(value));
}

void Ipv4Addr::setMcastLoop(
        const int  sd,
        const bool enable) const
{
    const unsigned char value = enable;
    if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &value, sizeof(value)))
        throw std::system_error(errno, std::system_category(),
                std::string("Couldn't set multicast packet looping: "
                "sock=") + std::to_string(sd) + ", enable=" +
                std::to_string(enable));
}

} // namespace
