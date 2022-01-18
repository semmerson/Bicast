/**
 * Internet address. Could be IPv4, IPv6, or a name.
 *
 *        File: InAddr.cpp
 *  Created on: May 6, 2019
 *      Author: Steven R. Emmerson
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
#include "InetAddr.h"
#include "SockAddr.h"
#include "Xprt.h"

#include <algorithm>
#include <arpa/inet.h>
#include <climits>
#include <cstring>
#include <functional>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#if defined(__linux__)
    #include <sys/types.h>
    #include <ifaddrs.h>
#endif

#ifndef _POSIX_HOST_NAME_MAX
#define _POSIX_HOST_NAME_MAX 255
#endif

namespace hycast {

class Inet4Addr;
class Inet6Addr;
class NameAddr;

class InetAddr::Impl
{
protected:
    /**
     * Returns an appropriate socket.
     *
     * @param[in] family       Address family. One of `AF_INET` or `AF_INET6`.
     * @param[in] type         Type of socket. One of `SOCK_STREAM`,
     *                         `SOCK_DGRAM`, or `SOCK_SEQPACKET`.
     * @param[in] protocol     Protocol. E.g., `IPPROTO_TCP` or `0` to obtain
     *                         the default protocol.
     * @return                 Appropriate socket
     * @throws    SystemError  `::socket()` failure
     */
    static int createSocket(
            const int family,
            const int type,
            const int protocol) {
        int sd = ::socket(family, type, protocol);

        if (sd == -1)
            throw SYSTEM_ERROR("::socket() failure: "
                    "family=" + std::to_string(family) + ","
                    "type=" + std::to_string(type) + ","
                    "protocol=" + std::to_string(protocol));

        return sd;
    }

public:
    enum AddrType : uint32_t {
        ADDR_IPV4,
        ADDR_IPV6,
        ADDR_NAME
    } addrType;

    template<typename TYPE>
    static TYPE* create(Xprt xprt) {
        auto impl = new TYPE();
        if (impl->read(xprt))
            return impl;
        delete impl;
        return nullptr;
    }

    virtual ~Impl() noexcept;

    virtual int getFamily() const noexcept =0;

    virtual std::string to_string() const =0;

    virtual bool operator<(const Impl& rhs) const noexcept =0;

    virtual bool operator<(const Inet4Addr& rhs) const noexcept =0;

    virtual bool operator<(const Inet6Addr& rhs) const noexcept =0;

    virtual bool operator<(const NameAddr& rhs) const noexcept =0;

    virtual bool operator==(const Impl& rhs) const noexcept =0;

    virtual bool operator==(const Inet4Addr& rhs) const noexcept =0;

    virtual bool operator==(const Inet6Addr& rhs) const noexcept =0;

    virtual bool operator==(const NameAddr& rhs) const noexcept =0;

    virtual size_t hash() const noexcept =0;

    virtual SockAddr getSockAddr(const in_port_t port) const =0;

    /**
     * Returns a socket descriptor appropriate to this instance's address
     * family.
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

    /**
     * Sets a socket address structure and returns a pointer to it.
     *
     * @param[in] storage  Socket address structure
     * @param[in] port     Port number in host byte-order
     * @return             Pointer to given socket address structure
     * @threadsafety       Safe
     */
    virtual struct sockaddr* get_sockaddr(
            struct sockaddr_storage& storage,
            const in_port_t          port) const =0;

    virtual void setMcastIface(int sd) const =0;

    virtual bool isAny() const =0;

    virtual bool isSsm() const =0;

    virtual bool write(Xprt xprt) const =0;

    /**
     * Joins the source-specific multicast group identified by this instance
     * and the address of the sending host.
     *
     * @param[in] sd       Socket identifier
     * @param[in] srcAddr  Address of the sending host
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  Maybe (`::getaddrinfo()` may be one and will be
     *                     called if either address is based on a name)
     */
    void join(
            const int       sd,
            const InetAddr& srcAddr) const
    {
        LOG_DEBUG("Joining multicast group %s from source %s",
                to_string().data(), srcAddr.to_string().data());

        // NB: The following is independent of protocol (i.e., IPv4 or IPv6)
        struct group_source_req mreq = {};

        mreq.gsr_interface = 0; // => O/S chooses interface
        getSockAddr(0).get_sockaddr(mreq.gsr_group);
        srcAddr.getSockAddr(0).get_sockaddr(mreq.gsr_source);

        if (::setsockopt(sd, IPPROTO_IP, MCAST_JOIN_SOURCE_GROUP, &mreq,
                sizeof(mreq)))
            throw SYSTEM_ERROR("Couldn't join multicast group " +
                    to_string() + " from source " + srcAddr.to_string());
    }
};

InetAddr::Impl::~Impl() noexcept
{}

/******************************************************************************/

class Inet4Addr final : public InetAddr::Impl
{
    struct in_addr       addr;
    std::hash<in_addr_t> myHash;

public:
    Inet4Addr(const in_addr_t addr) noexcept
        : Impl()
        , addr()
    {
        this->addr.s_addr = addr;
    }

    Inet4Addr()
        : Inet4Addr(INADDR_ANY)
    {}

    int getFamily() const noexcept
    {
        return AF_INET;
    }

    std::string to_string() const
    {
        char buf[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == nullptr)
            throw SYSTEM_ERROR("inet_ntop() failure");

        return std::string(buf);
    }

    bool operator <(const InetAddr::Impl& rhs) const noexcept
    {
        return !(rhs < *this) && !(rhs == *this);
    }

    bool operator <(const Inet4Addr& rhs) const noexcept
    {
        return ntohl(addr.s_addr) < ntohl(rhs.addr.s_addr);
    }

    bool operator <(const Inet6Addr& rhs) const noexcept
    {
        return true;
    }

    bool operator <(const NameAddr& rhs) const noexcept
    {
        return true;
    }

    bool operator ==(const InetAddr::Impl& rhs) const noexcept
    {
        return rhs == *this;
    }

    bool operator ==(const Inet4Addr& rhs) const noexcept
    {
        return addr.s_addr == rhs.addr.s_addr;
    }

    bool operator ==(const Inet6Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const NameAddr& rhs) const noexcept
    {
        return false;
    }

    size_t hash() const noexcept {
        return myHash(addr.s_addr);
    }

    int socket(
            const int type,
            const int protocol) const
    {
        return createSocket(AF_INET, type, protocol);
    }

    SockAddr getSockAddr(const in_port_t port) const
    {
        return SockAddr(addr.s_addr, port);
    }

    struct sockaddr* get_sockaddr(
            struct sockaddr_storage& storage,
            const in_port_t          port) const
    {
        ::memset(&storage, 0, sizeof(storage));
        struct sockaddr_in* const sockaddr =
                reinterpret_cast<struct sockaddr_in*>(&storage);
        sockaddr->sin_family = AF_INET;
        sockaddr->sin_addr = addr;
        sockaddr->sin_port = htons(port);
        return reinterpret_cast<struct sockaddr*>(sockaddr);
    }

    void setMcastIface(int sd) const override
    {
        LOG_DEBUG("Setting multicast interface for IPv4 UDP socket %d to %s",
                sd, to_string().data());
        if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) <
                0)
            throw SYSTEM_ERROR("Couldn't set multicast interface for IPv4 UDP "
                    "socket " + std::to_string(sd) + " to " + to_string());
    }

    bool isAny() const override {
        return ntohl(addr.s_addr) == INADDR_ANY;
    }

    bool isSsm() const override {
        auto ip = ntohl(addr.s_addr);
        return ip >= 0XE8000100 && ip <= 0XE8FFFFFF;
    }

    bool write(Xprt xprt) const {
        return xprt.write(ADDR_IPV4) && xprt.write(addr.s_addr);
    }

    bool read(Xprt xprt) {
        return xprt.read(addr.s_addr);
    }
};

/******************************************************************************/

class Inet6Addr final : public InetAddr::Impl
{
    struct in6_addr     addr;
    std::hash<uint64_t> myHash;

#if !defined(__linux__)
    unsigned getIfaceIndex(
            const char* buf,
            const int   len)
    {
        unsigned index = 0;

        for (const char* ptr = buf; ptr < buf + len; ) {
            const struct ifreq* ifr = reinterpret_cast<const struct ifreq*>(ptr);

            ptr += sizeof(ifr->ifr_name) + (
                    (ifr->ifr_addr.sa_family == AF_INET6)
                        ? sizeof(struct sockaddr_in6)
                        : sizeof(struct sockaddr));

            if (ifr->ifr_addr.sa_family == AF_LINK) {
                const struct sockaddr_dl *sdl =
                        static_cast<const struct sockaddr_dl*>(&ifr->ifr_addr);
                index = sdl->sdl_index;
            }

            if (ifr->ifr_addr.sa_family != AF_INET6)
                continue;

            if (memcmp(addr,
                    static_cast<const struct sockaddr_in6*>(&ifr->ifr_addr)->sin6_addr,
                    sizeof(addr)) == 0)
                break;
        }

        return index;
    }
#endif

    unsigned getIfaceIndex() const
    {
        unsigned index = 0; // 0 => no interface index found

#if defined(__linux__)
        struct ifaddrs* ifaddrs;

        if (::getifaddrs(&ifaddrs))
            throw SYSTEM_ERROR("Couldn't get information on interfaces");

        try {
            const struct ifaddrs* entry;

            for (entry = ifaddrs; entry; entry = entry->ifa_next) {
                const struct sockaddr* sockAddr = entry->ifa_addr;

                if (sockAddr->sa_family == AF_INET6) {
                    const struct in6_addr* in6Addr = &reinterpret_cast
                            <const struct sockaddr_in6*>(sockAddr)->sin6_addr;
                    if (::memcmp(&addr, in6Addr, sizeof(addr)) == 0)
                        break;
                }
            }

            if (entry)
                index = ::if_nametoindex(entry->ifa_name);

            ::freeifaddrs(ifaddrs);
        } // `ifaddrs` allocated
        catch (...) {
            ::freeifaddrs(ifaddrs);
        }
#else
        int      sock = ::socket(AF_INET, SOCK_DGRAM, 0);

        if (sock == -1)
            throw SYSTEM_ERROR("Couldn't create socket for ioctl()");
        try {
            ;
            int    lastlen = 0;

            for (int len = 100*sizeof(struct ifreq); ; len *= 2) {
                char buf[len];
                struct ifconf ifc =
                        {.ifc_ifcu={.ifcu_buf=buf}, .ifc_len=sizeof(buf)};

                if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
                    if (errno != EINVAL || lastlen)
                        throw SYSTEM_ERROR("ioctl() failure");
                }
                else {
                    if (ifc.ifc_len == lastlen) {
                        // Success, len has not changed
                        index = getIfaceIndex(buf, sizeof(ifc.ifc_name),
                                ifc.ifc_len);
                        break;
                    }
                    lastlen = ifc.ifc_len;
                }
            }

            close(sock);
        }
        catch (...) {
            ::close(sock);
        }
#endif

        return index;
    }

public:
    Inet6Addr(const struct in6_addr& addr) noexcept
        : Impl()
        , addr(addr)
    {}

    Inet6Addr()
        : Inet6Addr(in6addr_any)
    {}

    int getFamily() const noexcept
    {
        return AF_INET6;
    }

    std::string to_string() const
    {
        char buf[INET6_ADDRSTRLEN];

        if (inet_ntop(AF_INET6, &addr, buf, sizeof(buf)) == nullptr)
            throw SYSTEM_ERROR("inet_ntop() failure");

        return std::string(buf);
    }

    bool operator <(const InetAddr::Impl& rhs) const noexcept
    {
        return !(rhs < *this) && !(rhs == *this);
    }

    bool operator <(const Inet4Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator <(const Inet6Addr& rhs) const noexcept
    {
        return ::memcmp(&addr, &rhs.addr, sizeof(addr)) < 0;
    }

    bool operator <(const NameAddr& rhs) const noexcept
    {
        return true;
    }

    bool operator ==(const InetAddr::Impl& rhs) const noexcept
    {
        return rhs == *this;
    }

    bool operator ==(const Inet4Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const Inet6Addr& rhs) const noexcept
    {
        return ::memcmp(&addr, &rhs.addr, sizeof(addr)) == 0;
    }

    bool operator ==(const NameAddr& rhs) const noexcept
    {
        return false;
    }

    size_t hash() const noexcept {
        return myHash(*reinterpret_cast<const uint64_t*>(addr.s6_addr) ^
                      *reinterpret_cast<const uint64_t*>(addr.s6_addr+8));
    }

    int socket(
            const int type,
            const int protocol) const
    {
        return createSocket(AF_INET6, type, protocol);
    }

    SockAddr getSockAddr(const in_port_t port) const
    {
        return SockAddr(addr, port);
    }

    struct sockaddr* get_sockaddr(
            struct sockaddr_storage& storage,
            const in_port_t          port) const
    {
        ::memset(&storage, 0, sizeof(storage));
        struct sockaddr_in6* const sockaddr =
                reinterpret_cast<struct sockaddr_in6*>(&storage);
        sockaddr->sin6_family = AF_INET;
        sockaddr->sin6_addr = addr;
        sockaddr->sin6_port = htons(port);
        return reinterpret_cast<struct sockaddr*>(sockaddr);
    }

    void setMcastIface(int sd) const
    {
        unsigned ifaceIndex = getIfaceIndex();
        LOG_DEBUG("Setting multicast interface for IPv6 UDP socket %d to %u",
                sd, ifaceIndex);
        if (setsockopt(sd, IPPROTO_IP, IPV6_MULTICAST_IF, &ifaceIndex,
                       sizeof(ifaceIndex)) < 0)
            throw SYSTEM_ERROR("Couldn't set multicast interface for IPv6 UDP "
                    "socket " + std::to_string(sd) + " to " +
                    std::to_string(ifaceIndex));
    }

    bool isAny() const override {
        return ::memcmp(in6addr_any.s6_addr, addr.s6_addr, 16) == 0;
    }

    /*
     * FF3X::0000 through FF3X::4000:0000 or FF3X::8000:0000 through
     * FF3X::FFFF:FFFF (for IPv6).
     */
    bool isSsm() const override {
        // Get address in host byte-order
        uint8_t ip[16];
        if (htons(1) == 1) {
            ::memcpy(ip, addr.s6_addr, 16);
        }
        else {
            std::reverse_copy(addr.s6_addr, addr.s6_addr+16, ip);
        }
        ip[1] &= 0XF0; // Clear irrelevant bits

        // Check first 12 bytes
        static const uint8_t first12[12] = {0XFF, 0X30};
        if (::memcmp(ip, first12, 12))
            return false;

        // Check last 4 bytes
        const uint32_t last4 = (ip[12] << 24) | (ip[13] << 16) |
                (ip[14] << 8) | ip[15];
        return last4 <= 0X40000000 || last4 >= 0X80000000;
    }

    bool write(Xprt xprt) const {
        return xprt.write(ADDR_IPV6) &&
                xprt.write(addr.s6_addr, sizeof(addr.s6_addr));
    }

    bool read(Xprt xprt) {
        return xprt.read(addr.s6_addr, sizeof(addr.s6_addr));
    }
};

/******************************************************************************/

class NameAddr final : public InetAddr::Impl
{
    using SizeType = uint8_t; ///< Type for holding length of hostname

    std::string            name;
    std::hash<std::string> myHash;

    /**
     * Sets a socket address from the first IP-based Internet address that
     * matches the given information.
     *
     * @param[out] storage            Socket address
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
    bool get_sockaddr(
            struct sockaddr_storage& storage,
            const int                family,
            const in_port_t          port) const
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
                    auto*       dstaddr =
                            reinterpret_cast<struct sockaddr_in*>(&storage);
                    const auto* srcaddr = reinterpret_cast
                            <const struct sockaddr_in*>(entry->ai_addr);
                    *dstaddr = *srcaddr;
                    dstaddr->sin_port = htons(port);
                    success = true;
                    break;
                }
                else if (entry->ai_family == AF_INET6) {
                    auto*       dstaddr =
                            reinterpret_cast<struct sockaddr_in6*>(&storage);
                    const auto* srcaddr = reinterpret_cast
                            <const struct sockaddr_in6*>(entry->ai_addr);
                    *dstaddr = *srcaddr;
                    dstaddr->sin6_port = htons(port);
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

public:
    NameAddr(const std::string& name)
        : Impl()
        , name(name)
    {
        if (name.size() > _POSIX_HOST_NAME_MAX)
            throw INVALID_ARGUMENT("Name is longer than " +
                    std::to_string(_POSIX_HOST_NAME_MAX) + " bytes");
    }

    NameAddr()
        : NameAddr("")
    {}

    int getFamily() const noexcept
    {
        return AF_UNSPEC;
    }

    std::string to_string() const
    {
        return std::string(name);
    }

    bool operator <(const InetAddr::Impl& rhs) const noexcept
    {
        return !(rhs < *this) && !(rhs == *this);
    }

    bool operator <(const Inet4Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator <(const Inet6Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator <(const NameAddr& rhs) const noexcept
    {
        return name < rhs.name;
    }

    bool operator ==(const InetAddr::Impl& rhs) const noexcept
    {
        return rhs == *this;
    }

    bool operator ==(const Inet4Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const Inet6Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const NameAddr& rhs) const noexcept
    {
        return name == rhs.name;
    }

    size_t hash() const noexcept {
        return myHash(name);
    }

    int socket(
            const int type,
            const int protocol) const
    {
        struct sockaddr_storage storage;
        return createSocket(get_sockaddr(storage, 0)->sa_family, type,
                protocol);
    }

    SockAddr getSockAddr(const in_port_t port) const
    {
        return SockAddr(name, port);
    }

    struct sockaddr* get_sockaddr(
            struct sockaddr_storage& storage,
            const in_port_t          port) const
    {
        if (!get_sockaddr(storage, AF_INET, port) &&
            !get_sockaddr(storage, AF_INET6, port))
                throw RUNTIME_ERROR(
                        "Couldn't get IP address for \"" + name + "\"");
        return reinterpret_cast<struct sockaddr*>(&storage);
    }

    void setMcastIface(int sd) const
    {
        struct sockaddr_storage storage;
        get_sockaddr(storage, 0);

        if (storage.ss_family == AF_INET) {
            const auto* sockaddr =
                    reinterpret_cast<struct sockaddr_in*>(&storage);
            Inet4Addr(sockaddr->sin_addr.s_addr).setMcastIface(sd);
        }
        else if (storage.ss_family == AF_INET6) {
            const auto* sockaddr =
                    reinterpret_cast<struct sockaddr_in6*>(&storage);
            Inet6Addr(sockaddr->sin6_addr).setMcastIface(sd);
        }
        else {
            throw LOGIC_ERROR("Unsupported address family: " +
                    std::to_string(storage.ss_family));
        }
    }

    bool isAny() const override {
        return false;
    }

    bool isSsm() const override {
        return false;
    }

    bool write(Xprt xprt) const {
        return xprt.write(static_cast<SizeType>(name.size())) &&
                xprt.write(name.data(), name.size());
    }

    bool read(Xprt xprt) {
        SizeType nbytes;
        auto success = xprt.read(nbytes);
        if (success) {
            if (nbytes > _POSIX_HOST_NAME_MAX)
                throw RUNTIME_ERROR("Name is longer than " +
                        std::to_string(_POSIX_HOST_NAME_MAX) + " bytes");
            char bytes[nbytes];
            success = xprt.read(bytes, nbytes);
            if (success)
                name.assign(bytes, nbytes);
        }
        return success;
    }
};

/******************************************************************************/

InetAddr::InetAddr() noexcept
    : pImpl()
{}

InetAddr::InetAddr(const in_addr_t addr) noexcept
    : pImpl(new Inet4Addr(addr))
{}

InetAddr::InetAddr(const struct in_addr& addr) noexcept
    : InetAddr(addr.s_addr)
{}

InetAddr::InetAddr(const struct in6_addr& addr) noexcept
    : pImpl(new Inet6Addr(addr))
{}

InetAddr::InetAddr(const std::string& addr)
    : pImpl()
{
    const char*     cstr = addr.data();
    struct in_addr  in4addr;
    struct in6_addr in6addr;

    if (::inet_pton(AF_INET, cstr, &in4addr) == 1) {
        pImpl.reset(new Inet4Addr(in4addr.s_addr));
    }
    else if (::inet_pton(AF_INET6, cstr, &in6addr) == 1) {
        pImpl.reset(new Inet6Addr(in6addr));
    }
    else {
        pImpl.reset(new NameAddr(addr));
    }
}

InetAddr::operator bool() const noexcept
{
    return static_cast<bool>(pImpl);
}

int InetAddr::getFamily() const noexcept
{
    return pImpl->getFamily();
}

std::string InetAddr::to_string() const
{
    return pImpl ? pImpl->to_string() : "<unset>";
}

bool InetAddr::operator<(const InetAddr& rhs) const noexcept
{
    auto impl1 = pImpl.get();
    auto impl2 = rhs.pImpl.get();
    return (impl1 == impl2)
            ? false
            : (impl1 == nullptr || impl2 == nullptr)
                  ? (impl1 == nullptr)
                  : *impl1 < *impl2;
}

bool InetAddr::operator==(const InetAddr& rhs) const noexcept
{
    return !(*pImpl < *rhs.pImpl || *rhs.pImpl < *pImpl);
}

size_t InetAddr::hash() const noexcept
{
    return pImpl ? pImpl->hash() : 0;
}

SockAddr InetAddr::getSockAddr(const in_port_t port) const
{
    return pImpl->getSockAddr(port);
}

int InetAddr::socket(
        const int type,
        const int protocol) const
{
    return pImpl->socket(type, protocol);
}

void InetAddr::join(
        const int       sd,
        const InetAddr& srcAddr) const
{
    return pImpl->join(sd, srcAddr);
}

struct sockaddr* InetAddr::get_sockaddr(
        struct sockaddr_storage& storage,
        const in_port_t          port) const
{
    return pImpl->get_sockaddr(storage, port);
}

const InetAddr& InetAddr::setMcastIface(int sd) const
{
    pImpl->setMcastIface(sd);
    return *this;
}

bool InetAddr::isAny() const
{
    return pImpl->isAny();
}

bool InetAddr::isSsm() const
{
    return pImpl->isSsm();
}

bool InetAddr::write(Xprt xprt) const {
    return pImpl->write(xprt);
}

bool InetAddr::read(Xprt xprt) {
    uint8_t addrType;
    Impl*   impl;

    if (!xprt.read(addrType))
        return nullptr;

    if (addrType == Impl::ADDR_IPV4) {
        impl = Impl::create<Inet4Addr>(xprt);
    }
    else if (addrType == Impl::ADDR_IPV6) {
        impl = Impl::create<Inet6Addr>(xprt);
    }
    else if (addrType == Impl::ADDR_NAME) {
        impl = Impl::create<NameAddr>(xprt);
    }
    else {
        throw RUNTIME_ERROR("Unsupported address type: " +
                std::to_string(addrType));
    }

    if (impl == nullptr)
        return false;

    pImpl.reset(impl);
    return true;
}

} // namespace
