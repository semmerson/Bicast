/**
 * This file implements a network interface.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Interface.cpp
 *  Created on: May 8, 2017
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "Interface.h"

#include <sys/types.h>
#include <ifaddrs.h>

namespace hycast {

class Interface::Impl final
{
    std::string name;

public:
    /**
     * Prevents default construction.
     */
    Impl() =delete;

    /**
     * Constructs.
     * @param[in] name  Name of the interface (e.g., "eth0")
     */
    Impl(const std::string& name)
    	: name{name}
    {}

    /**
     * Constructs.
     * @param[in] index  Index of the interface (e.g., 2)
     */
    Impl(const int index)
    {
        throw LOGIC_ERROR("Not implemented yet");
    }

    /**
     * Prevents copy and move construction and assignment.
     */
    Impl(const Impl& that) =delete;
    Impl(const Impl&& that) =delete;
    Impl& operator=(const Impl& rhs) =delete;
    Impl& operator=(const Impl&& rhs) =delete;

    /**
     * Returns an Internet address of the interface.
     * @param[in] family       Address family. One of `AF_INET` or `AF_INET6`.
     * @return                 Corresponding Internet address of interface
     * @throw InvalidArgument  `family` is invalid
     * @throw SystemError      Error getting information on interfaces
     * @throw RuntimeError     Couldn't construct Internet address
     * @throw LogicError       No information on interface
     * @exceptionsafety        Strong guarantee
     * @threadsafety           Probably compatible but not safe
     */
    InetAddr getInetAddr(const int family) const
    {
        /*
         * This implementation depends on Linux semantics for the non-portable
         * function `getifaddrs(3)`. The BSD version apparently has different
         * semantics.
         */
        if (family != AF_INET && family != AF_INET6)
            throw INVALID_ARGUMENT("Unsupported address family: " +
                    std::to_string(family));
        struct ifaddrs* list;
        if (::getifaddrs(&list))
            throw SYSTEM_ERROR(
                    "Couldn't get information on network interfaces", errno);
        try {
            for (struct ifaddrs* entry = list; entry != NULL;
                    entry = entry->ifa_next) {
                if (entry->ifa_name && name.compare(entry->ifa_name) == 0) {
                    struct sockaddr* sockaddr = entry->ifa_addr;
                    if (sockaddr && sockaddr->sa_family == family) {
                        auto inetAddr = (family == AF_INET)
                                ? InetAddr{reinterpret_cast<struct sockaddr_in*>
                                                (sockaddr)->sin_addr}
                                : InetAddr{reinterpret_cast<struct sockaddr_in6*>
                                                (sockaddr)->sin6_addr};
                        ::freeifaddrs(list);
                        return inetAddr;
                    }
                }
            }
            ::freeifaddrs(list);
        }
        catch (const std::exception& e) {
            ::freeifaddrs(list);
            std::throw_with_nested(RUNTIME_ERROR(
                    "Couldn't construct Internet address"));
        }
        throw LOGIC_ERROR("No relevant information: iface=" + name +
                ", addrFamily=" + std::to_string(family));
    }
};

Interface::Interface(const std::string& name)
    : pImpl{new Impl(name)}
{}

Interface::Interface(const int number)
    : pImpl{new Impl(number)}
{
    throw LOGIC_ERROR("Not implemented yet");
}

InetAddr hycast::Interface::getInetAddr(const int family) const
{
    return pImpl->getInetAddr(family);
}

} // namespace
