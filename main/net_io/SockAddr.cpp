/**
 * Socket address. Can be IPv4, IPv6, or UNIX domain.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: SockAddr.cpp
 *  Created on: May 12, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "SockAddr.h"

#include <arpa/inet.h>
#include <climits>
#include <netdb.h>
#include <regex>
#include <stdio.h>
#include <sys/socket.h>

namespace hycast {

class SockAddr::Impl
{
protected:
    InAddr      inAddr;
    in_port_t   port;
    std::string strRep;

public:
    Impl()
        : inAddr{}
        , port{0}
    {}

    Impl(   const InAddr& inAddr,
            const in_port_t port)
        : inAddr{inAddr}
        , port{port}
        , strRep{}
    {
        if (port > 65535)
            throw INVALID_ARGUMENT("Port number is too large: " +
                    std::to_string(port));
    }

    virtual ~Impl() =0;

    const InAddr& getInAddr() const noexcept
    {
        return inAddr;
    }

    virtual const Impl* clone(in_port_t port) const =0;

    const std::string& to_string() const noexcept
    {
        return strRep;
    }

    bool operator <(const Impl& rhs) const
    {
        return (inAddr < rhs.inAddr)
                ? true
                : (rhs.inAddr < inAddr)
                  ? false
                  : (port < rhs.port);
    }

    virtual void getSockAddr(
            struct sockaddr& sockaddr,
            socklen_t&       socklen) const =0;

    in_port_t getPort() const
    {
        return port;
    }

    virtual int getFamily() const =0;
};

SockAddr::Impl::~Impl()
{}

/******************************************************************************/

class SockAddrIn::Impl final : public SockAddr::Impl
{
private:
    struct sockaddr_in  sockaddr;

public:
    Impl()
        : sockaddr{0}
    {}

    Impl(
            const in_addr_t addr,
            const in_port_t port) ///< Port number in host byte-order
        : SockAddr::Impl{InAddr(addr), port}
        , sockaddr{0}
    {
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons(port);
        sockaddr.sin_addr.s_addr = addr;

        strRep = inAddr.to_string() + ":" + std::to_string(port);
    }

    const Impl* clone(in_port_t port) const
    {
        return new Impl(sockaddr.sin_addr.s_addr, port);
    }

    void getSockAddr(
            struct sockaddr& sockaddr,
            socklen_t&       socklen) const
    {
        socklen = sizeof(this->sockaddr);
        ::memcpy(&sockaddr, &this->sockaddr, socklen);
    }

    int getFamily() const noexcept
    {
        return AF_INET;
    }
};

SockAddrIn::SockAddrIn()
    : SockAddr{}
{}

SockAddrIn::SockAddrIn(
        const in_addr_t addr,
        const in_port_t port)
    : SockAddr{new Impl(addr, port)}
{}

/******************************************************************************/

class SockAddrIn6::Impl final : public SockAddr::Impl
{
private:
    struct sockaddr_in6 sockaddr;

public:
    Impl()
        : sockaddr{0}
    {}

    Impl(
            const struct in6_addr& addr,
            const in_port_t        port) ///< Port number in host byte-order
        : SockAddr::Impl{InAddr(addr), port}
        , sockaddr{0}
    {
        sockaddr.sin6_family = AF_INET6;
        sockaddr.sin6_port = htons(port);
        sockaddr.sin6_addr = addr;

        strRep = "[" + inAddr.to_string() + "]:" + std::to_string(port);
    }

    const Impl* clone(in_port_t port) const
    {
        return new Impl(sockaddr.sin6_addr, port);
    }

    void getSockAddr(
            struct sockaddr& sockaddr,
            socklen_t&       socklen) const
    {
        socklen = sizeof(this->sockaddr);
        ::memcpy(&sockaddr, &this->sockaddr, socklen);
    }

    int getFamily() const noexcept
    {
        return AF_INET6;
    }
};

SockAddrIn6::SockAddrIn6()
    : SockAddr{}
{}

SockAddrIn6::SockAddrIn6(
        const struct in6_addr& addr,
        const in_port_t        port)
    : SockAddr{new Impl(addr, port)}
{}

/******************************************************************************/

class SockAddrName::Impl final : public SockAddr::Impl
{
private:
    std::string         name;
    mutable SockAddr    sockAddr;

    /**
     * Sets a socket address from the first IP-based Internet address that
     * matches the given information.
     *
     * @param[out] sockaddr           Socket address
     * @param[in]  name               Name of host
     * @param[in]  family             Address family. One of `AF_INET` or
     *                                `AF_INET6`.
     * @param[in]  port               Port number in host byte-order
     * @retval     `true`             Success. `sockaddr` is set.
     * @retval     `false`            Failure. `sockaddr` is not set.
     * @throws     std::system_error  `::getaddrinfo()` failure
     * @exceptionsafety               Strong guarantee
     * @threadsafety                  Safe
     */
    static bool setSockAddr(
            SockAddr&          sockAddr,
            const std::string& name,
            const int          family,
            const in_port_t    port)
    {
        bool             success = false;
        struct addrinfo  hints = {};
        struct addrinfo* list;

        hints.ai_family = family;
        hints.ai_socktype = 0;

        if (::getaddrinfo(name.data(), nullptr, &hints, &list))
            throw SYSTEM_ERROR(
                    std::string("::getaddrinfo() failure for host \"") +
                    name.data() + "\"");
        try {
            for (struct addrinfo* entry = list; entry != NULL;
                    entry = entry->ai_next) {
                if (entry->ai_family == AF_INET) {
                    const struct sockaddr_in* sockAddrIn = reinterpret_cast<
                            const struct sockaddr_in*>(entry->ai_addr);

                    sockAddr = SockAddr(sockAddrIn->sin_addr.s_addr, port);
                    success = true;
                    break;
                }
                else if (entry->ai_family == AF_INET6) {
                    const struct sockaddr_in6* sockAddrIn6 = reinterpret_cast<
                            const struct sockaddr_in6*>(entry->ai_addr);

                    sockAddr = SockAddr(sockAddrIn6->sin6_addr, port);
                    success = true;
                    break;
                }
            }

            freeaddrinfo(list);
            return success;
        }
        catch (...) {
            freeaddrinfo(list);
            throw;
        }
    }

    /**
     * Sets the socket address from the first IP-based Internet address
     * associated with this instance's hostname.
     *
     * @throws std::system_error   `::getaddrinfo()` failure
     * @throws std::runtime_error  Couldn't get IP address
     * @exceptionsafety            Strong guarantee
     * @threadsafety               Safe
     */
    void setSockAddr() const
    {
        if (!setSockAddr(sockAddr, name, AF_INET, port) &&
            !setSockAddr(sockAddr, name, AF_INET6, port))
                throw RUNTIME_ERROR(
                        "Couldn't get IP address for \"" + name + "\"");
    }

public:
    Impl()
        : SockAddr::Impl{}
        , name{}
        , sockAddr{}
    {}

    Impl(
            const std::string name,
            const in_port_t   port) ///< Port number in host byte-order
        : SockAddr::Impl{InAddr(name), port}
        , name{name}
        , sockAddr{}
    {
        strRep = name + ":" + std::to_string(port);
    }

    const Impl* clone(in_port_t port) const
    {
        return new Impl(name, port);
    }

    void getSockAddr(
            struct sockaddr& sockaddr,
            socklen_t&       socklen) const
    {
        setSockAddr();
        sockAddr.getSockAddr(sockaddr, socklen);
    }

    int getFamily() const
    {
        setSockAddr();
        sockAddr.getFamily();
    }
};

SockAddrName::SockAddrName()
    : SockAddr{}
{}

SockAddrName::SockAddrName(
        const std::string& name,
        const in_port_t    port)
    : SockAddr{new Impl(name, port)}
{}

/******************************************************************************/

SockAddr::SockAddr() noexcept
    : pImpl()
{}

SockAddr::SockAddr(const Impl* const impl)
    : pImpl{impl}
{}

SockAddr::SockAddr(
        const in_addr_t addr,
        const in_port_t port) ///< Port number in host byte-order
     : pImpl{new SockAddrIn::Impl(addr, port)}
{}

SockAddr::SockAddr(
        const struct in_addr& addr,
        const in_port_t       port) ///< Port number in host byte-order
     : pImpl{new SockAddrIn::Impl(addr.s_addr, port)}
{}

SockAddr::SockAddr(const struct sockaddr_in& sockaddr)
     : pImpl{new SockAddrIn::Impl(sockaddr.sin_addr.s_addr,
             ntohs(sockaddr.sin_port))}
{}

SockAddr::SockAddr(
        const struct in6_addr& addr,
        const in_port_t        port) ///< Port number in host byte-order
    : pImpl{new SockAddrIn6::Impl(addr, port)}
{}

SockAddr::SockAddr(const struct sockaddr_in6& sockaddr)
    : pImpl{new SockAddrIn6::Impl(sockaddr.sin6_addr, ntohs(sockaddr.sin6_port))}
{}

SockAddr::SockAddr(
        const std::string& addr,
        const in_port_t    port)
    : pImpl{}
{
    const char*     cstr{addr.data()};
    struct in_addr  inaddr;
    struct in6_addr in6addr;

    if (inet_pton(AF_INET, cstr, &inaddr) == 1) {
        pImpl.reset(new SockAddrIn::Impl(inaddr.s_addr, port));
    }
    else if (inet_pton(AF_INET6, cstr, &in6addr) == 1) {
        pImpl.reset(new SockAddrIn6::Impl(in6addr, port));
    }
    else {
        pImpl.reset(new SockAddrName::Impl(addr, port));
    }
}

static bool parseSpec(
        const char* const  spec,
        const char* const  pattern,
        char*&             ident,
        in_port_t&         portNum)
{
    char*          id = NULL;
    unsigned long  port; // Doesn't work under gcc 4.8.5 if `short`
    int            numAssign;
    bool           success = false;
    int            nbytes = -1;

    // The following doesn't work under gcc 4.8.5
    //if (::sscanf(spec, "%m[0-9.]:%hu%n", &id, &port, &nbytes) == 2)
    // `SCNu16` succeeds on "99999" :-(
    numAssign = ::sscanf(spec, pattern, &id, &port, &nbytes);

    if (numAssign != 2) {
        free(id);
    }
    else if (nbytes < 0) {
        free(id);
        throw INVALID_ARGUMENT("Format pattern doesn't contain \"%n\"");
    }
    else {
        if (spec[nbytes]) {
            free(id);
            throw INVALID_ARGUMENT(std::string("Excess characters: \"") + spec
                    + "\"");
        }

        if (port > USHRT_MAX) {
            free(id);
            throw INVALID_ARGUMENT("Port number is too large: " +
                    std::to_string(port));
        }

        ident = id;
        portNum = static_cast<in_port_t>(port);
        success = true;
    }

    return success;
}

SockAddr::SockAddr(const std::string& spec)
{
    // std::regex in gcc 4.8 doesn't work; hence, the following

    const char*    cstr = spec.data();
    char*          id = NULL;
    in_port_t      port; // Doesn't work under gcc 4.8.5 if `short`

    try {
        if (parseSpec(cstr, "%m[0-9.]:%5lu%n", id, port)) {
            in_addr_t addr;

            if (::inet_pton(AF_INET, id, &addr) != 1)
                throw INVALID_ARGUMENT(std::string(
                        "Invalid IPv4 specification: \"") + id + "\"");

            pImpl.reset(new SockAddrIn::Impl(addr,
                    static_cast<in_port_t>(port)));
        }
        else if (parseSpec(cstr, "[%m[0-9a-fA-F:]]:%5lu%n", id, port)) {
            struct in6_addr addr;

            if (::inet_pton(AF_INET6, id, &addr) != 1)
                throw INVALID_ARGUMENT(std::string(
                        "Invalid IPv6 specification: \"") + id + "\"");

            pImpl.reset(new SockAddrIn6::Impl(addr,
                    static_cast<in_port_t>(port)));
        }
        else if (parseSpec(cstr, "%m[0-9a-zA-Z._-]:%5lu%n", id, port)) {
            pImpl.reset(new SockAddrName::Impl(id,
                    static_cast<in_port_t>(port)));
        }
        else {
            throw INVALID_ARGUMENT("Invalid socket address: \"" + spec + "\"");
        }

        free(id);
    }
    catch (const std::exception& ex) {
        free(id);
        throw;
    }
}

SockAddr SockAddr::clone(const in_port_t port) const
{
    return SockAddr(pImpl->clone(port));
}

bool SockAddr::operator <(const SockAddr& rhs) const
{
    return *pImpl.get() < *rhs.pImpl.get();
}

const std::string& SockAddr::to_string() const noexcept
{
    return pImpl->to_string();
}

void SockAddr::getSockAddr(
        struct sockaddr& sockaddr,
        socklen_t&       socklen) const
{
    return pImpl->getSockAddr(sockaddr, socklen);
}

const InAddr& SockAddr::getInAddr() const noexcept
{
    return pImpl->getInAddr();
}

in_port_t SockAddr::getPort() const
{
    return pImpl->getPort();
}

int SockAddr::getFamily() const
{
    return pImpl->getFamily();
}

} // namespace

/******************************************************************************/

namespace std {
    template<>
    struct less<hycast::SockAddr> {
        bool operator()(
            const hycast::SockAddr& lhs,
            const hycast::SockAddr& rhs)
        {
            return lhs < rhs;
        }
    };
}
