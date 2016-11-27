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

void InetNameAddr::getSockAddrs(
        const int                        family,
        const in_port_t                  port,
        std::set<struct sockaddr>* const set) const
{
    struct addrinfo  hints = {};
    hints.ai_family = family;
    hints.ai_protocol = IPPROTO_SCTP;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* list;
    if (::getaddrinfo(name.data(), nullptr, &hints, &list))
        throw std::system_error(errno, std::system_category(),
                std::string("::getaddrinfo(AF_INET) failure for host \"") +
                name.data() + "\"");
    try {
        for (struct addrinfo* entry = list; entry->ai_next != NULL;
                entry = entry->ai_next) {
            if (entry->ai_addr->sa_family == AF_INET) {
                (reinterpret_cast<struct sockaddr_in*>(entry->ai_addr))->
                        sin_port = htons(port);
            }
            else if (entry->ai_addr->sa_family == AF_INET6) {
                (reinterpret_cast<struct sockaddr_in6*>(entry->ai_addr))->
                        sin6_port = htons(port);
            }
            set->insert(*entry->ai_addr);
        }
    }
    catch (...) {
        freeaddrinfo(list);
        throw;
    }
}

std::shared_ptr<std::set<struct sockaddr>> InetNameAddr::getSockAddr(
        const in_port_t  port) const
{
    auto set = new std::set<struct sockaddr>();
    if (set == nullptr)
        throw std::system_error(errno, std::system_category(),
                "Couldn't allocate set for socket address");
    getSockAddrs(AF_INET, port, set);
    getSockAddrs(AF_INET6, port, set);
    return std::shared_ptr<std::set<struct sockaddr>>(set);
}

} // namespace
