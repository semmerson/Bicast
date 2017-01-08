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

#include <cstring>
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

void InetNameAddr::getSockAddrs(
        const int                                family,
        const in_port_t                          port,
        std::set<struct sockaddr_storage>* const set) const
{
    struct addrinfo  hints = {};
    hints.ai_family = family;
    struct addrinfo* list;
    if (::getaddrinfo(name.data(), nullptr, &hints, &list))
        throw std::system_error(errno, std::system_category(),
                std::string("::getaddrinfo() failure for host \"") +
                name.data() + "\"");
    try {
        for (struct addrinfo* entry = list; entry->ai_next != NULL;
                entry = entry->ai_next) {
            union {
                struct sockaddr_in      in4;
                struct sockaddr_in6     in6;
                struct sockaddr_storage storage;
            } sockaddr = {};
            if (entry->ai_addr->sa_family == AF_INET ||
                    entry->ai_addr->sa_family == AF_INET6) {
                ::memcpy(&sockaddr, entry->ai_addr, entry->ai_addrlen);
                if (entry->ai_addr->sa_family == AF_INET) {
                    sockaddr.in4.sin_port = htons(port);
                }
                else if (entry->ai_addr->sa_family == AF_INET6) {
                    sockaddr.in6.sin6_port = htons(port);
                }
                set->insert(sockaddr.storage);
            }
        }
    }
    catch (...) {
        freeaddrinfo(list);
        throw;
    }
}

std::shared_ptr<std::set<struct sockaddr_storage>> InetNameAddr::getSockAddr(
        const in_port_t  port) const
{
    auto set = new std::set<struct sockaddr_storage>();
    if (set == nullptr)
        throw std::system_error(errno, std::system_category(),
                "Couldn't allocate set for socket address");
    getSockAddrs(AF_INET, port, set);
    getSockAddrs(AF_INET6, port, set);
    return std::shared_ptr<std::set<struct sockaddr_storage>>(set);
}

} // namespace
