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

/// Implementation of a socket
class SockAddr::Impl
{
protected:
    InetAddr             inetAddr; ///< IP address
    in_port_t            port;     ///< Port number in host byte-order
    std::hash<in_port_t> portHash; ///< Hash code of the port number

public:
    Impl()
        : inetAddr{}
        , port{0}
    {}

    /**
     * Constructs.
     * @param[in] inetAddr  IP address
     * @param[in] port      Port number
     */
    Impl(   const InetAddr& inetAddr,
            const in_port_t port)
        : inetAddr{inetAddr}
        , port{port}
    {
        if (port > 65535)
            throw INVALID_ARGUMENT("Port number is too large: " +
                    std::to_string(port));
    }

    /**
     * Returns the IP address component.
     * @return The IP address component
     */
    const InetAddr getInetAddr() const noexcept
    {
        return inetAddr;
    }

    /**
     * Returns the port number.
     * @return The port number
     */
    in_port_t getPort() const noexcept
    {
        return port;
    }

    /**
     * Returns the string representation of this instance.
     * @param[in] withName  Should the name of this class be included?
     * @return              The string representation of this instance
     */
    std::string to_string(const bool withName = false) const noexcept
    {
        return (withName ? "SockAddr{" : "") +
                ((inetAddr.getFamily() == AF_INET6)
                    ? "[" + inetAddr.to_string() + "]:" + std::to_string(port)
                    : inetAddr.to_string() + ":" + std::to_string(port)) +
               (withName ? "}" : "");
    }

    /**
     * Indicates if this instance is less than another.
     * @param[in] rhs      The other, right-hand-side instance
     * @retval    true     This instance is less than the other
     * @retval    false    This instance is not less than the other
     */
    inline bool operator<(const Impl& rhs) const noexcept {
        return (inetAddr < rhs.inetAddr)
                ? true
                : (rhs.inetAddr < inetAddr)
                  ? false
                  : (port < rhs.port);
    }

    /**
     * Indicates if this instance is not equal to another.
     * @param[in] rhs      The other, right-hand-side instance
     * @retval    true     This instance is not equal to the other
     * @retval    false    This instance is equal to the other
     */
    inline bool operator!=(const Impl& rhs) const noexcept {
        return (*this < rhs) || (rhs < *this);
    }

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs      The other, right-hand-side instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    inline bool operator==(const Impl& rhs) const noexcept {
        return !(*this != rhs);
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
     * Sets and returns a socket address structure.
     * @param[out] storage  Storage for the socket address
     * @return              Pointer to `storage` as a socket address structure
     */
    struct sockaddr* get_sockaddr(struct sockaddr_storage& storage) const
    {
        inetAddr.get_sockaddr(storage, port);
        return reinterpret_cast<struct sockaddr*>(&storage);
    }

    /**
     * Returns a socket appropriate to this instance's address family.
     *
     * @param[in] type               Type of socket. One of `SOCK_STREAM`,
     *                               `SOCK_DGRAM`, or `SOCK_SEQPACKET`.
     * @param[in] protocol           Protocol. E.g., `IPPROTO_TCP` or `0` to
     *                               obtain the default protocol.
     * @return                       Appropriate socket
     * @throws    std::system_error  Couldn't create socket
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
     * @throws    std::system_error  Bind failure
     * @threadsafety                 Safe
     */
    void bind(const int sd) const
    {
        struct sockaddr_storage storage;

        //LOG_NOTE("Binding socket " + std::to_string(sd) + " to " + to_string());
        struct sockaddr* sockaddr = inetAddr.get_sockaddr(storage, port);
        //LOG_NOTE("sockaddr->sa_family=" + std::to_string(sockaddr->sa_family));
        if (::bind(sd, sockaddr, sizeof(storage)))
            throw SYSTEM_ERROR("Couldn't bind socket " + std::to_string(sd) + " to " + to_string());
    }

    /**
     * Writes itself to a transport.
     * @param[in] xprt  The transport
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(Xprt xprt) const {
        return inetAddr.write(xprt) && xprt.write(port);
    }

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
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

/**
 * Constructs from an IPv4 socket address.
 *
 * @param[in] addr  IPv4 address
 * @param[in] port  Port number in host byte-order. `0` obtains a system-chosen port number.
 */
SockAddr::SockAddr(
        const in_addr_t addr,
        const in_port_t port)
     : SockAddr(InetAddr(addr), port)
{}

/**
 * Constructs from an IPv4 socket address.
 *
 * @param[in] addr  IPv4 address
 * @param[in] port  Port number in host byte-order. `0` obtains a system-chosen port number.
 */
SockAddr::SockAddr(
        const struct in_addr& addr,
        const in_port_t       port)
     : SockAddr(InetAddr(addr.s_addr), port)
{}

SockAddr::SockAddr(const struct sockaddr_in& sockaddr)
     : SockAddr(InetAddr(sockaddr.sin_addr.s_addr), ntohs(sockaddr.sin_port))
{}

SockAddr::SockAddr(
        const struct in6_addr& addr,
        const in_port_t        port)
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

/**
 * Splits a socket specification into Internet and port number specifications
 * @param[in]  spec        Socket specification
 * @param[out] inet        Internet specification
 * @param[out] port        Port number specification
 * @throw InvalidArgument  Not a socket specification
 */
static void splitSpec(
        const String& spec,
        String&       inet,
        String&       port)
{
    auto pos = spec.rfind(':');

    if (pos == spec.npos)
        throw INVALID_ARGUMENT("No port specified in \"" + spec + "\"");

    if (pos >= 3 && spec[pos-1] == ']') {
        // "[" <IPv6 addr> "]:" <port>
        inet = spec.substr(1, pos-2);
        port = spec.substr(pos+1);
    }
    else if (pos >= 1) {
        // (<IPv4 addr>|<hostname>) ":" <port>
        inet = spec.substr(0, pos);
        port = spec.substr(pos+1);
    }
    else {
        throw INVALID_ARGUMENT("Not a socket specification: \"" + spec + "\"");
    }
}

SockAddr::SockAddr(const std::string& spec)
    : SockAddr{}
{
    // std::regex in gcc 4.8 doesn't work; hence, the following

    String inetSpec = "", portSpec = "";

    splitSpec(spec, inetSpec, portSpec);

    InetAddr      inet{inetSpec};
    unsigned long port;

    if (::sscanf(portSpec.data(), "%lu", &port) != 1)
        throw INVALID_ARGUMENT("Invalid port specification: \"" + portSpec + "\"");
    if (port > USHRT_MAX)
        throw INVALID_ARGUMENT("Port number is too large: " + std::to_string(port));

    pImpl.reset(new Impl(inet, static_cast<in_port_t>(port)));
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

const InetAddr SockAddr::getInetAddr() const noexcept
{
    return pImpl->getInetAddr();
}

in_port_t SockAddr::getPort() const noexcept
{
    return pImpl->getPort();
}

struct sockaddr* SockAddr::get_sockaddr(struct sockaddr_storage& storage) const
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

/**
 * Writes a socket address to an output stream.
 * @param[in] ostream  The output stream
 * @param[in] addr
 * @return
 */
std::ostream& operator<<(std::ostream& ostream, const SockAddr& addr) {
    return ostream << addr.to_string();
}

} // namespace

namespace std {
    /**
     * Returns the string representation of a socket address.
     * @param[in] sockAddr  Socket address
     * @return              String representation of socket address
     * @see SockAddr::to_string()
     */
    string to_string(const hycast::SockAddr& sockAddr) {
        return sockAddr.to_string();
    }
}
