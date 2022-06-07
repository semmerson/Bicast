/**
 * Socket address. Can be IPv4 or IPv6.
 *
 *        File: SockAddr.cpp
 *  Created on: May 12, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include "error.h"
#include "SockAddr.h"
#include "Xprt.h"

#include <arpa/inet.h>
#include <climits>
#include <fcntl.h>
#include <functional>
#include <net/if.h>
#include <netdb.h>
#include <poll.h>
#include <regex>
#include <stdio.h>
#include <sys/socket.h>

namespace hycast {

class SockAddr::Impl
{
protected:
    InetAddr             inetAddr;
    in_port_t            port;     // Port number in host byte-order
    std::hash<in_port_t> portHash;

public:
    Impl()
        : inetAddr{}
        , port{0}
    {}

    Impl(   const InetAddr& inetAddr,
            const in_port_t port)
        : inetAddr{inetAddr}
        , port{port}
    {
        if (port > 65535)
            throw INVALID_ARGUMENT("Port number is too large: " +
                    std::to_string(port));
    }

    const InetAddr getInetAddr() const noexcept
    {
        return inetAddr;
    }

    in_port_t getPort() const noexcept
    {
        return port;
    }

    std::string to_string(const bool withName = false) const noexcept
    {
        return (withName ? "SockAddr{" : "") +
                ((inetAddr.getFamily() == AF_INET6)
                    ? "[" + inetAddr.to_string() + "]:" + std::to_string(port)
                    : inetAddr.to_string() + ":" + std::to_string(port)) +
               (withName ? "}" : "");
    }

    bool operator<(const Impl& rhs) const noexcept
    {
        return (inetAddr < rhs.inetAddr)
                ? true
                : (rhs.inetAddr < inetAddr)
                  ? false
                  : (port < rhs.port);
    }

    bool operator==(const Impl& rhs) const
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

    void get_sockaddr(struct sockaddr_storage& storage) const
    {
        inetAddr.get_sockaddr(storage, port);
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
            const int protocol) const
    {
        return inetAddr.socket(type, protocol);
    }

    /**
     * Binds a socket to a local socket address. Address is server's
     * listening address/outgoing IP source field or incoming multicast
     * destination address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::bind()` failure
     * @threadsafety                 Safe
     */
    void bind(const int sd) const
    {
        struct sockaddr_storage storage;

        LOG_DEBUG("Binding socket " + std::to_string(sd) + " to " + to_string());
        if (::bind(sd, inetAddr.get_sockaddr(storage, port), sizeof(storage)))
            throw SYSTEM_ERROR("Couldn't bind socket " + std::to_string(sd) + " to " + to_string());
    }

    /**
     * Connects a socket to this address within a timeout.
     *
     * @param[in] sd           Socket descriptor
     * @param[in] timeout      Timeout in ms. -1 => indefinite timeout; 0 => immediate return.
     * @throw InvalidArgument  `timeout < -1`
     * @throw RuntimeError     Timeout occurred
     * @throw SystemError      System failure
     */
    void connect(
            const int sd,
            const int timeout) const {
        LOG_DEBUG("Connecting socket %d to %s", sd, to_string().data());

        if (timeout < -1)
            throw INVALID_ARGUMENT("Timeout < -1 ms: " + std::to_string(timeout));

        // Get original socket flags
        int origSockFlags = ::fcntl(sd, F_GETFL, 0);
        if (origSockFlags < 0)
            throw SYSTEM_ERROR("Couldn't get socket flags");

        // Set O_NONBLOCK
        if (::fcntl(sd, F_SETFL, origSockFlags | O_NONBLOCK) == -1)
            throw SYSTEM_ERROR("Couldn't make socket non-blocking");

        // Start connecting asynchronously
        struct sockaddr_storage storage;
        if (::connect(sd, inetAddr.get_sockaddr(storage, port), sizeof(storage)) &&
                errno != EINPROGRESS)
            throw SYSTEM_ERROR("connect() failure");

        struct pollfd pfd = {.fd=sd, .events=POLLOUT};
        int           status = ::poll(&pfd, 1, timeout);

        if (status == -1)
            throw SYSTEM_ERROR("poll() failure");
        if (status == 0)
            throw RUNTIME_ERROR("Couldn't connect to " + to_string() + " in " +
                    std::to_string(timeout) + " ms");

        // Restore original socket flags
        if (fcntl(sd, F_SETFL, origSockFlags) == -1)
            throw SYSTEM_ERROR("Couldn't restore socket flags");
    }

    /**
     * Connects a socket to this address.
     *
     * @param[in] sd            Socket descriptor
     * @throws    SystemError   System failure
     * @threadsafety            Safe
     */
    void connect(const int sd) const {
        connect(sd, -1);
    }

    /**
     * Joins the source-specific multicast group identified by this instance
     * and the address of the sending host.
     *
     * @param[in] sd       Socket identifier
     * @param[in] srcAddr  Address of the sending host
     * @param[in] ifAddr   Interface to use
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  Maybe (`::getaddrinfo()` may be one and will be
     *                     called if either address is based on a name)
     */
    void join(
            const int          sd,
            const InetAddr&    srcAddr,
            const std::string& iface) const
    {
        LOG_DEBUG("Joining multicast group %s from source %s on interface %s",
                to_string().data(), srcAddr.to_string().data(), iface.data());

        // NB: The following is independent of protocol (i.e., IPv4 or IPv6)
        struct group_source_req mreq = {};

        mreq.gsr_interface = ::if_nametoindex(iface.data());
        inetAddr.get_sockaddr(mreq.gsr_group, port);
        srcAddr.get_sockaddr(mreq.gsr_source, 0);

        if (::setsockopt(sd, IPPROTO_IP, MCAST_JOIN_SOURCE_GROUP, &mreq,
                sizeof(mreq)))
            throw SYSTEM_ERROR("Couldn't join multicast group " +
                    to_string() + " from source " + srcAddr.to_string());
    }

    bool write(Xprt xprt) const {
        return inetAddr.write(xprt) && xprt.write(port);
    }

    bool read(Xprt xprt) {
        return inetAddr.read(xprt) && xprt.read(port);
    }
};

/******************************************************************************/

SockAddr::SockAddr() noexcept
    : pImpl()
{}

SockAddr::SockAddr(
        const InetAddr& inetAddr,
        in_port_t       port)
    : pImpl{new Impl(inetAddr, port)}
{}

SockAddr::SockAddr(
        const in_addr_t addr,
        const in_port_t port) ///< Port number in host byte-order
     : SockAddr(InetAddr(addr), port)
{}

SockAddr::SockAddr(
        const struct in_addr& addr,
        const in_port_t       port) ///< Port number in host byte-order
     : SockAddr(InetAddr(addr.s_addr), port)
{}

SockAddr::SockAddr(const struct sockaddr_in& sockaddr)
     : SockAddr(InetAddr(sockaddr.sin_addr.s_addr), ntohs(sockaddr.sin_port))
{}

SockAddr::SockAddr(
        const struct in6_addr& addr,
        const in_port_t        port) ///< Port number in host byte-order
    : SockAddr(InetAddr(addr), port)
{}

SockAddr::SockAddr(const struct sockaddr_in6& sockaddr)
    : SockAddr(InetAddr(sockaddr.sin6_addr), ntohs(sockaddr.sin6_port))
{}

SockAddr::SockAddr(const struct sockaddr_storage& storage)
    : pImpl()
{
    if (storage.ss_family == AF_INET) {
        const struct sockaddr_in* addr =
                reinterpret_cast<const struct sockaddr_in*>(&storage);
        pImpl.reset(new Impl(InetAddr(addr->sin_addr), ntohs(addr->sin_port)));
    }
    else if (storage.ss_family == AF_INET6) {
        const struct sockaddr_in6* addr =
                reinterpret_cast<const struct sockaddr_in6*>(&storage);
        pImpl.reset(new Impl(InetAddr(addr->sin6_addr),
                ntohs(addr->sin6_port)));
    }
    else {
        throw INVALID_ARGUMENT("Unsupported address family: " +
                std::to_string(storage.ss_family));
    }
    //LOG_DEBUG("%s", pImpl->to_string().c_str());
}

SockAddr::SockAddr(const struct sockaddr& sockaddr)
    : SockAddr{*reinterpret_cast<const struct sockaddr_storage*>(&sockaddr)}
{}

SockAddr::SockAddr(
        const std::string& addr,
        const in_port_t    port)
    : SockAddr{}
{
    const char*     cstr{addr.data()};
    struct in_addr  inaddr;
    struct in6_addr in6addr;

    if (inet_pton(AF_INET, cstr, &inaddr) == 1) {
        pImpl.reset(new Impl(InetAddr(inaddr), port));
    }
    else if (inet_pton(AF_INET6, cstr, &in6addr) == 1) {
        pImpl.reset(new Impl(InetAddr(in6addr), port));
    }
    else {
        pImpl.reset(new Impl(InetAddr(addr), port));
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
    : SockAddr{}
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

            pImpl.reset(new Impl(InetAddr(addr), port));
        }
        else if (parseSpec(cstr, "[%m[0-9a-fA-F:]]:%5lu%n", id, port)) {
            struct in6_addr addr;

            if (::inet_pton(AF_INET6, id, &addr) != 1)
                throw INVALID_ARGUMENT(std::string(
                        "Invalid IPv6 specification: \"") + id + "\"");

            pImpl.reset(new Impl(addr, port));
        }
        else if (parseSpec(cstr, "%m[0-9a-zA-Z._-]:%5lu%n", id, port)) {
            pImpl.reset(new Impl(InetAddr(id), port));
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
    return SockAddr{getInetAddr(), port};
}

SockAddr::operator bool() const noexcept
{
    return static_cast<bool>(pImpl);
}

bool SockAddr::operator<(const SockAddr& rhs) const noexcept
{
    auto impl1 = pImpl.get();
    auto impl2 = rhs.pImpl.get();
    return (impl1 == impl2)
            ? false
            : (impl1 == nullptr || impl2 == nullptr)
                ? (impl1 == nullptr)
                : *impl1 < *impl2;
}

bool SockAddr::operator==(const SockAddr& rhs) const noexcept
{
    return *pImpl == *rhs.pImpl;
}

size_t SockAddr::hash() const noexcept
{
    return pImpl ? pImpl->hash() : 0;
}

std::string SockAddr::to_string(const bool withName) const noexcept
{
    return pImpl
            ? pImpl->to_string(withName)
            : withName
                  ? "SockAddr{<unset>}"
                  : "<unset>";
}

void SockAddr::bind(const int sd) const
{
    pImpl->bind(sd);
}

void SockAddr::connect(
        const int sd,
        const int timeout) const
{
    return pImpl->connect(sd, timeout);
}

void SockAddr::connect(const int sd) const
{
    return pImpl->connect(sd);
}

void SockAddr::join(
        const int          sd,
        const InetAddr&    srcAddr,
        const std::string& iface) const
{
    pImpl->join(sd, srcAddr, iface);
}

const InetAddr SockAddr::getInetAddr() const noexcept
{
    return pImpl->getInetAddr();
}

in_port_t SockAddr::getPort() const noexcept
{
    return pImpl->getPort();
}

void SockAddr::get_sockaddr(struct sockaddr_storage& storage) const
{
    return pImpl->get_sockaddr(storage);
}

bool SockAddr::write(Xprt xprt) const {
    return pImpl->write(xprt);
}

bool SockAddr::read(Xprt xprt) {
    pImpl.reset(new Impl());
    return pImpl->read(xprt);
}

std::ostream& operator<<(std::ostream& ostream, const SockAddr& addr) {
    return ostream << addr.to_string();
}

} // namespace

namespace std {
    string to_string(const hycast::SockAddr& sockAddr) {
        return sockAddr.to_string();
    }
}
