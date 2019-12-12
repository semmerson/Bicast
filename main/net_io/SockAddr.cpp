/**
 * Socket address. Can be IPv4 or IPv6.
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
#include <functional>
#include <netdb.h>
#include <regex>
#include <stdio.h>
#include <sys/socket.h>

namespace hycast {

class SockAddr::Impl
{
protected:
    InetAddr             inetAddr;
    in_port_t            port;
    std::string          strRep;
    std::hash<in_port_t> portHash;

    /**
     * Returns an appropriate socket.
     *
     * @param[in] family             Address family. One of `AF_INET` or
     *                               `AF_INET6`.
     * @param[in] type               Type of socket. One of `SOCK_STREAM`,
     *                               `SOCK_DGRAM`, or `SOCK_SEQPACKET`.
     * @param[in] protocol           Protocol. E.g., `IPPROTO_TCP` or `0` to
     *                               obtain the default protocol.
     * @return                       Appropriate socket
     * @throws    std::system_error  `::socket()` failure
     */
    static int createSocket(
            const int family,
            const int type,
            const int protocol) {
        int sd = ::socket(family, type, protocol);

        if (sd == -1)
            throw SYSTEM_ERROR("::socket() failure: {"
                    "family: " + std::to_string(family) + ", "
                    "type: " + std::to_string(type) + ", "
                    "protocol: " + std::to_string(protocol) + "}");

        return sd;
    }

public:
    Impl()
        : inetAddr{}
        , port{0}
    {}

    Impl(   const InetAddr& inAddr,
            const in_port_t port)
        : inetAddr{inAddr}
        , port{port}
        , strRep{}
    {
        if (port > 65535)
            throw INVALID_ARGUMENT("Port number is too large: " +
                    std::to_string(port));
    }

    virtual ~Impl() =0;

    const InetAddr& getInetAddr() const noexcept
    {
        return inetAddr;
    }


    /**
     * Returns a socket appropriate to this instance's address family.
     *
     * @param[in] type               Type of socket. One of `SOCK_STREAM`,
     *                               `SOCK_DGRAM`, or `SOCK_SEQPACKET`.
     * @param[in] protocol           Protocol. E.g., `IPPROTO_TCP` or `0` to
     *                               obtain the default protocol.
     * @return                       Appropriate socket
     * @throws    std::system_error  `::socket()` failure
     */
    virtual int socket(
            const int type,
            const int protocol) const =0;

    virtual const Impl* clone(in_port_t port) const =0;

    const std::string& to_string() const noexcept
    {
        return strRep;
    }

    bool operator <(const Impl& rhs) const
    {
        return (inetAddr < rhs.inetAddr)
                ? true
                : (rhs.inetAddr < inetAddr)
                  ? false
                  : (port < rhs.port);
    }

    bool operator ==(const Impl& rhs) const
    {
        return !((*this < rhs) || (rhs < *this));
    }

    /**
     * Returns the hash value of this instance.
     *
     * @return The hash value of this instance
     */
    size_t hash() const noexcept {
        return inetAddr.hash() ^ portHash(port);
    }

    /**
     * Binds a socket to a local socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::bind()` failure
     * @threadsafety                 Safe
     */
    virtual void bind(const int sd) const =0;

    /**
     * Connects a socket to a remote socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::connect()` failure
     * @threadsafety                 Safe
     */
    virtual void connect(const int sd) const =0;

    in_port_t getPort() const noexcept
    {
        return port;
    }

    virtual void setAddr(struct sockaddr_storage& storage) const =0;
};

SockAddr::Impl::~Impl()
{}

/******************************************************************************/

class SockAddrIn final : public SockAddr::Impl
{
private:
    struct sockaddr_in  sockaddr;

public:
    SockAddrIn()
        : Impl()
        , sockaddr{0}
    {}

    SockAddrIn(const struct sockaddr_in& sockaddr)
        : Impl(InetAddr(sockaddr.sin_addr), ntohs(sockaddr.sin_port))
        , sockaddr(sockaddr)
    {
        strRep = inetAddr.to_string() + ":" + std::to_string(port);
    }

    SockAddrIn(
            const in_addr_t addr,
            const in_port_t port) ///< Port number in host byte-order
        : Impl{InetAddr(addr), port}
        , sockaddr{0}
    {
        LOG_DEBUG("{addr: %s, port: %hu}", inetAddr.to_string().c_str(), port);
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons(port);
        sockaddr.sin_addr.s_addr = addr;

        strRep = inetAddr.to_string() + ":" + std::to_string(port);
    }

    /**
     * Returns a socket appropriate to this instance's address family.
     *
     * @param[in] type               Type of socket. One of `SOCK_STREAM`,
     *                               `SOCK_DGRAM`, or `SOCK_SEQPACKET`.
     * @param[in] protocol           Protocol. E.g., `IPPROTO_TCP` or `0` to
     *                               obtain the default protocol.
     * @return                       Appropriate socket
     * @throws    std::system_error  `::socket()` failure
     */
    int socket(
            const int type,
            const int protocol) const {
        return createSocket(AF_INET, type, protocol);
    }

    const SockAddrIn* clone(in_port_t port) const
    {
        return new SockAddrIn(sockaddr.sin_addr.s_addr, port);
    }

    /**
     * Binds a socket to this instance's address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::bind()` failure
     * @threadsafety                 Safe
     */
    void bind(const int sd) const {
        if (::bind(sd, reinterpret_cast<const struct sockaddr*>(&sockaddr),
                sizeof(sockaddr)))
            throw SYSTEM_ERROR("Couldn't bind socket " + std::to_string(sd) +
                    " to " + strRep);
    }

    /**
     * Connects a socket to a remote socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::connect()` failure
     * @threadsafety                 Safe
     */
    void connect(const int sd) const {
        if (::connect(sd, reinterpret_cast<const struct sockaddr*>(&sockaddr),
                sizeof(sockaddr)))
            throw SYSTEM_ERROR("Couldn't connect socket " + std::to_string(sd) +
                    " to " + strRep);
    }

    void setAddr(struct sockaddr_storage& storage) const
    {
        ::memcpy(&storage, &sockaddr, sizeof(sockaddr));
    }
};

/******************************************************************************/

class SockAddrIn6 final : public SockAddr::Impl
{
private:
    struct sockaddr_in6 sockaddr;

public:
    SockAddrIn6()
        : Impl()
        , sockaddr{0}
    {}

    SockAddrIn6(const struct sockaddr_in6& sockaddr)
        : Impl(InetAddr(sockaddr.sin6_addr), ntohs(sockaddr.sin6_port))
        , sockaddr(sockaddr)
    {
        strRep = "[" + inetAddr.to_string() + "]:" + std::to_string(port);
    }

    SockAddrIn6(
            const struct in6_addr& addr,
            const in_port_t        port) ///< Port number in host byte-order
        : Impl{InetAddr(addr), port}
        , sockaddr{0}
    {
        sockaddr.sin6_family = AF_INET6;
        sockaddr.sin6_port = htons(port);
        sockaddr.sin6_addr = addr;

        strRep = "[" + inetAddr.to_string() + "]:" + std::to_string(port);
    }

    /**
     * Returns a socket appropriate for this instance's address family.
     *
     * @param[in] type               Type of socket. One of `SOCK_STREAM`,
     *                               `SOCK_DGRAM`, or `SOCK_SEQPACKET`.
     * @param[in] protocol           Protocol. E.g., `IPPROTO_TCP` or `0` to
     *                               obtain the default protocol.
     * @return                       Appropriate socket
     * @throws    std::system_error  `::socket()` failure
     */
    int socket(
            const int type,
            const int protocol) const {
        return createSocket(AF_INET6, type, protocol);
    }

    const SockAddrIn6* clone(in_port_t port) const
    {
        return new SockAddrIn6(sockaddr.sin6_addr, port);
    }

    /**
     * Binds a socket to this instance's socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::bind()` failure
     * @threadsafety                 Safe
     */
    void bind(const int sd) const {
        if (::bind(sd, reinterpret_cast<const struct sockaddr*>(&sockaddr),
                sizeof(sockaddr)))
            throw SYSTEM_ERROR("Couldn't bind socket " + std::to_string(sd) +
                    " to " + strRep);
    }

    /**
     * Connects a socket to a remote socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::connect()` failure
     * @threadsafety                 Safe
     */
    void connect(const int sd) const {
        if (::connect(sd, reinterpret_cast<const struct sockaddr*>(&sockaddr),
                sizeof(sockaddr)))
            throw SYSTEM_ERROR("Couldn't connect socket " + std::to_string(sd) +
                    " to " + strRep);
    }

    void setAddr(struct sockaddr_storage& storage) const
    {
        ::memcpy(&storage, &sockaddr, sizeof(sockaddr));
    }
};

/******************************************************************************/

class SockAddrName final : public SockAddr::Impl
{
private:
    std::string         name;

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
     * @cancellationpoint             Maybe (`::getaddrinfo()` may be one)
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
     * Sets a socket address from the first IP-based Internet address
     * associated with this instance's hostname.
     *
     * @param[out] sockAddr        Socket address to be set
     * @throws std::system_error   `::getaddrinfo()` failure
     * @throws std::runtime_error  Couldn't get IP address
     * @exceptionsafety            Strong guarantee
     * @threadsafety               Safe
     * @cancellationpoint          Maybe (`::getaddrinfo()` may be one)
     */
    void setSockAddr(SockAddr& sockAddr) const
    {
        if (!setSockAddr(sockAddr, name, AF_INET, port) &&
            !setSockAddr(sockAddr, name, AF_INET6, port))
                throw RUNTIME_ERROR(
                        "Couldn't get IP address for \"" + name + "\"");
    }

public:
    SockAddrName()
        : Impl{}
        , name{}
    {}

    SockAddrName(
            const std::string name,
            const in_port_t   port) ///< Port number in host byte-order
        : SockAddr::Impl{InetAddr(name), port}
        , name{name}
    {
        strRep = name + ":" + std::to_string(port);
    }

    /**
     * Returns a socket appropriate for this instance's address family.
     *
     * @param[in] type               Type of socket. One of `SOCK_STREAM`,
     *                               `SOCK_DGRAM`, or `SOCK_SEQPACKET`.
     * @param[in] protocol           Protocol. E.g., `IPPROTO_TCP` or `0` to
     *                               obtain the default protocol.
     * @return                       Appropriate socket
     * @throws    std::system_error  `::socket()` failure
     */
    int socket(
            const int type,
            const int protocol) const
    {
        SockAddr sockAddr;
        setSockAddr(sockAddr);
        return sockAddr.socket(type, protocol);
    }

    const SockAddrName* clone(in_port_t port) const
    {
        return new SockAddrName(name, port);
    }

    /**
     * Binds a socket to a local socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::bind()` failure
     * @threadsafety                 Safe
     */
    void bind(const int sd) const
    {
        SockAddr sockAddr;
        setSockAddr(sockAddr);
        sockAddr.bind(sd);
    }

    /**
     * Connects a socket to a remote socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::connect()` failure
     * @threadsafety                 Safe
     */
    void connect(const int sd) const
    {
        SockAddr sockAddr;
        setSockAddr(sockAddr);
        sockAddr.connect(sd);
    }

    void setAddr(struct sockaddr_storage& storage) const
    {
        SockAddr sockAddr;
        setSockAddr(sockAddr);
        sockAddr.setAddr(storage);
    }
};

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
     : pImpl{new SockAddrIn(addr, port)}
{}

SockAddr::SockAddr(
        const struct in_addr& addr,
        const in_port_t       port) ///< Port number in host byte-order
     : pImpl{new SockAddrIn(addr.s_addr, port)}
{}

SockAddr::SockAddr(const struct sockaddr_in& sockaddr)
     : pImpl{new SockAddrIn(sockaddr.sin_addr.s_addr,
             ntohs(sockaddr.sin_port))}
{}

SockAddr::SockAddr(
        const struct in6_addr& addr,
        const in_port_t        port) ///< Port number in host byte-order
    : pImpl{new SockAddrIn6(addr, port)}
{}

SockAddr::SockAddr(const struct sockaddr_in6& sockaddr)
    : pImpl{new SockAddrIn6(sockaddr.sin6_addr, ntohs(sockaddr.sin6_port))}
{}

SockAddr::SockAddr(const struct sockaddr& sockaddr)
    : pImpl{}
{
    if (sockaddr.sa_family == AF_INET) {
        const struct sockaddr_in* addr =
                reinterpret_cast<const struct sockaddr_in*>(&sockaddr);
        pImpl.reset(new SockAddrIn(*addr));
    }
    else if (sockaddr.sa_family == AF_INET6) {
        const struct sockaddr_in6* addr =
                reinterpret_cast<const struct sockaddr_in6*>(&sockaddr);
        pImpl.reset(new SockAddrIn6(*addr));
    }
    else {
        throw INVALID_ARGUMENT("Unsupported address family: " +
                std::to_string(sockaddr.sa_family));
    }
    //LOG_DEBUG("%s", pImpl->to_string().c_str());
}

SockAddr::SockAddr(
        const std::string& addr,
        const in_port_t    port)
    : pImpl{}
{
    const char*     cstr{addr.data()};
    struct in_addr  inaddr;
    struct in6_addr in6addr;

    if (inet_pton(AF_INET, cstr, &inaddr) == 1) {
        pImpl.reset(new SockAddrIn(inaddr.s_addr, port));
    }
    else if (inet_pton(AF_INET6, cstr, &in6addr) == 1) {
        pImpl.reset(new SockAddrIn6(in6addr, port));
    }
    else {
        pImpl.reset(new SockAddrName(addr, port));
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

            pImpl.reset(new SockAddrIn(addr,
                    static_cast<in_port_t>(port)));
        }
        else if (parseSpec(cstr, "[%m[0-9a-fA-F:]]:%5lu%n", id, port)) {
            struct in6_addr addr;

            if (::inet_pton(AF_INET6, id, &addr) != 1)
                throw INVALID_ARGUMENT(std::string(
                        "Invalid IPv6 specification: \"") + id + "\"");

            pImpl.reset(new SockAddrIn6(addr,
                    static_cast<in_port_t>(port)));
        }
        else if (parseSpec(cstr, "%m[0-9a-zA-Z._-]:%5lu%n", id, port)) {
            pImpl.reset(new SockAddrName(id,
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

int SockAddr::socket(
            const int type,
            const int protocol) const
{
    return pImpl->socket(type, protocol);
}

SockAddr SockAddr::clone(const in_port_t port) const
{
    return SockAddr(pImpl->clone(port));
}

bool SockAddr::operator <(const SockAddr& rhs) const
{
    return *pImpl.get() < *rhs.pImpl.get();
}

bool SockAddr::operator ==(const SockAddr& rhs) const
{
    return *pImpl.get() == *rhs.pImpl.get();
}

size_t SockAddr::hash() const noexcept
{
    return pImpl->hash();
}

const std::string& SockAddr::to_string() const noexcept
{
    return pImpl->to_string();
}

void SockAddr::bind(const int sd) const
{
    pImpl->bind(sd);
}

void SockAddr::connect(const int sd) const
{
    pImpl->connect(sd);
}

const InetAddr& SockAddr::getInetAddr() const noexcept
{
    return pImpl->getInetAddr();
}

in_port_t SockAddr::getPort() const noexcept
{
    return pImpl->getPort();
}

void SockAddr::setAddr(struct sockaddr_storage& storage) const
{
    return pImpl->setAddr(storage);
}

} // namespace
