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
    virtual const void* getAddr(socklen_t* size) const =0;

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
    enum AddrType : uint8_t {
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

    Impl(AddrType addrType)
        : addrType(addrType)
    {}

    virtual ~Impl() noexcept;

    virtual int getFamily() const noexcept =0;

    AddrType getAddrType() const noexcept {
        return addrType;
    }

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

    /**
     * Makes the given socket use the interface associated with this instance.
     *
     * @param[in] sd          UDP socket descriptor
     * @return                This instance
     * @throws    LogicError  This instance is based on a hostname and not an IP address
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     Unknown due to non-standard function usage
     */
    virtual void makeIface(int sd) const =0;

    virtual bool isMemberOf(const struct sockaddr* ifa_netmask) const =0;

    /**
     * Returns the index of the interface that has this address.
     *
     * @throw LogicError  No interface has this address
     * @return            Interface index associated with this address
     */
    unsigned getIfaceIndex() const
    {
        unsigned index = 0; // 0 => No corresponding interface

        if (!isAny()) {
            struct ifaddrs* ifaddrs;
            if (::getifaddrs(&ifaddrs))
                throw SYSTEM_ERROR("Couldn't get information on interfaces");

            try {
                const struct ifaddrs* entry;
                socklen_t             size;
                const void*           targetAddr = getAddr(&size);
                const int             targetFamily = getFamily();

                for (entry = ifaddrs; entry; entry = entry->ifa_next) {
                    const struct sockaddr* ifaceAddr = entry->ifa_addr;

                    if (ifaceAddr && ifaceAddr->sa_family == targetFamily) {
                        if (targetFamily == AF_INET6 && ::memcmp(targetAddr,
                                &reinterpret_cast<const struct sockaddr_in6*>(ifaceAddr)->sin6_addr,
                                size) == 0) {
                            break;
                        }
                        else if (::memcmp(targetAddr,
                                &reinterpret_cast<const struct sockaddr_in*>(ifaceAddr)->sin_addr,
                                size) == 0) {
                            break;
                        }
                    }
                }

                if (!entry) {
                    char buf[INET6_ADDRSTRLEN] = {};
                    throw LOGIC_ERROR("No interface has address " +
                            std::string(::inet_ntop(targetFamily, targetAddr, buf, sizeof(buf))));
                }
                else {
                    index = ::if_nametoindex(entry->ifa_name);
                }

                ::freeifaddrs(ifaddrs);
            } // `ifaddrs` allocated
            catch (...) {
                ::freeifaddrs(ifaddrs);
            }
        }

        return index;
    }

    virtual bool isAny() const =0;

    virtual bool isMulticast() const =0;

    virtual bool isSsm() const =0;

    virtual bool write(Xprt xprt) const =0;
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
        : Impl(ADDR_IPV4)
        , addr()
    {
        this->addr.s_addr = addr;
    }

    Inet4Addr()
        : Inet4Addr(INADDR_ANY)
    {}

    Inet4Addr(Xprt xprt)
        : Inet4Addr()
    {
        if (!read(xprt))
            throw RUNTIME_ERROR("Constructor failure");
    }

    int getFamily() const noexcept
    {
        return AF_INET;
    }

    const void* getAddr(socklen_t* size) const override {
        *size = sizeof(addr);
        return &addr;
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
        struct sockaddr_in* const sockaddr = reinterpret_cast<struct sockaddr_in*>(&storage);
        sockaddr->sin_family = AF_INET;
        sockaddr->sin_addr = addr;
        sockaddr->sin_port = htons(port);
        return reinterpret_cast<struct sockaddr*>(sockaddr);
    }

    void makeIface(int sd) const override
    {
        LOG_DEBUG("Setting multicast interface for IPv4 UDP socket %d to %s",
                sd, to_string().data());
        if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) <
                0)
            throw SYSTEM_ERROR("Couldn't set multicast interface for IPv4 UDP "
                    "socket " + std::to_string(sd) + " to " + to_string());
    }

    bool isMemberOf(const struct sockaddr* netmask) const override {
        const struct sockaddr_in* maskAddr = reinterpret_cast<const struct sockaddr_in*>(netmask);
        return maskAddr->sin_family == AF_INET &&
                (addr.s_addr & maskAddr->sin_addr.s_addr) == maskAddr->sin_addr.s_addr;
    }

    bool isAny() const override {
        return addr.s_addr == htonl(INADDR_ANY);
    }

    bool isMulticast() const override {
        auto ip = ntohl(addr.s_addr);
        return ip >= 0XE0000000 && ip <= 0XEFFFFFFF;
    }

    bool isSsm() const override {
        auto ip = ntohl(addr.s_addr);
        return ip >= 0XE8000100 && ip <= 0XE8FFFFFF;
    }

    bool write(Xprt xprt) const {
        return xprt.write(addr.s_addr);
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

#if 0
    unsigned getIfaceIndex() const
    {
        unsigned index = 0; // 0 => no interface index found
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

        return index;
#endif

public:
    Inet6Addr(const struct in6_addr& addr) noexcept
        : Impl(ADDR_IPV6)
        , addr(addr)
    {}

    Inet6Addr()
        : Inet6Addr(in6addr_any)
    {}

    Inet6Addr(Xprt xprt)
        : Inet6Addr()
    {
        if (!read(xprt))
            throw RUNTIME_ERROR("Constructor failure");
    }

    int getFamily() const noexcept
    {
        return AF_INET6;
    }

    const void* getAddr(socklen_t* size) const override {
        *size = sizeof(addr);
        return &addr;
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

    void makeIface(int sd) const override
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

    bool isMemberOf(const struct sockaddr* netmask) const override {
        const struct sockaddr_in6* maskAddr = reinterpret_cast<const struct sockaddr_in6*>(netmask);
        if (maskAddr->sin6_family != AF_INET6)
            return false;
        for (int i = 0; i < sizeof(addr.s6_addr); ++i)
            if (addr.s6_addr[i] & maskAddr->sin6_addr.s6_addr[i] != maskAddr->sin6_addr.s6_addr[i])
                return false;
        return true;
    }

    bool isAny() const override {
        return ::memcmp(in6addr_any.s6_addr, addr.s6_addr, 16) == 0;
    }

    bool isMulticast() const override {
        return IN6_IS_ADDR_MULTICAST(&addr);
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
        return xprt.write(addr.s6_addr, sizeof(addr.s6_addr));
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
        : Impl(ADDR_NAME)
        , name(name)
    {
        if (name.size() > _POSIX_HOST_NAME_MAX)
            throw INVALID_ARGUMENT("Name is longer than " +
                    std::to_string(_POSIX_HOST_NAME_MAX) + " bytes");
    }

    NameAddr()
        : NameAddr("")
    {}

    NameAddr(Xprt xprt)
        : NameAddr()
    {
        if (!read(xprt))
            throw RUNTIME_ERROR("NameAddr(Xprt) failure");
    }

    int getFamily() const noexcept
    {
        return AF_UNSPEC;
    }

    const void* getAddr(socklen_t* size) const override {
        *size = name.size();
        return name.data();
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

    void makeIface(int sd) const override
    {
        struct sockaddr_storage storage;
        get_sockaddr(storage, 0);

        if (storage.ss_family == AF_INET) {
            const auto* sockaddr =
                    reinterpret_cast<struct sockaddr_in*>(&storage);
            Inet4Addr(sockaddr->sin_addr.s_addr).makeIface(sd);
        }
        else if (storage.ss_family == AF_INET6) {
            const auto* sockaddr =
                    reinterpret_cast<struct sockaddr_in6*>(&storage);
            Inet6Addr(sockaddr->sin6_addr).makeIface(sd);
        }
        else {
            throw LOGIC_ERROR("Unsupported address family: " +
                    std::to_string(storage.ss_family));
        }
    }

    bool isMemberOf(const struct sockaddr* netmask) const override {
        return false;
    }

    bool isAny() const override {
        return false;
    }

    bool isMulticast() const override {
        return false;
    }

    bool isSsm() const override {
        return false;
    }

    bool write(Xprt xprt) const {
        return xprt.write<SizeType>(name);
    }

    bool read(Xprt xprt) {
        auto success = xprt.read<SizeType>(name);
        if (success && name.size() > _POSIX_HOST_NAME_MAX)
            throw RUNTIME_ERROR("Hostname is longer than " +
                    std::to_string(_POSIX_HOST_NAME_MAX) + " bytes");
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

struct sockaddr* InetAddr::get_sockaddr(
        struct sockaddr_storage& storage,
        const in_port_t          port) const
{
    return pImpl->get_sockaddr(storage, port);
}

const InetAddr& InetAddr::makeIface(int sd) const
{
    pImpl->makeIface(sd);
    return *this;
}

unsigned InetAddr::getIfaceIndex() const
{
    return pImpl->getIfaceIndex();
}

bool InetAddr::isAny() const
{
    return pImpl->isAny();
}

bool InetAddr::isMulticast() const
{
    return pImpl->isMulticast();
}

bool InetAddr::isSsm() const
{
    return pImpl->isSsm();
}

bool InetAddr::write(Xprt xprt) const {
    uint8_t addrType = pImpl->getAddrType();
    return xprt.write(addrType) && pImpl->write(xprt);
}

bool InetAddr::read(Xprt xprt) {
    uint8_t addrType;
    Impl*   impl;

    if (!xprt.read(addrType))
        return false;

    if (addrType == Impl::ADDR_IPV4) {
        impl = new Inet4Addr(xprt);
        //impl = Impl::create<Inet4Addr>(xprt);
    }
    else if (addrType == Impl::ADDR_IPV6) {
        impl = new Inet6Addr(xprt);
        //impl = Impl::create<Inet6Addr>(xprt);
    }
    else if (addrType == Impl::ADDR_NAME) {
        impl = new NameAddr(xprt);
        //impl = Impl::create<NameAddr>(xprt);
    }
    else {
        throw RUNTIME_ERROR("Unsupported address type: " +
                std::to_string(addrType));
    }

    pImpl.reset(impl);
    return true;
}

std::ostream& operator<<(std::ostream& ostream, const hycast::InetAddr& addr) {
    return ostream << addr.to_string();
}

} // namespace

namespace std {
}
