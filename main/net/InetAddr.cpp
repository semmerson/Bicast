/**





 * This file implements an Internet address, which may be based on an IPv4
 * address, an IPv6 address, or a hostname.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddr.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "InetAddr.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <set>
#include <sys/ioctl.h>

namespace std {
    template<>
    struct less<struct sockaddr_storage> {
        bool operator()(
            const struct sockaddr_storage sockaddr1,
            const struct sockaddr_storage sockaddr2)
        {
            return ::memcmp(&sockaddr1, &sockaddr2, sizeof(sockaddr1)) < 0;
        }
    };
}

namespace hycast {

class IpAddr;
class Ipv4Addr;
class Ipv6Addr;
class InetNameAddr;

class InetAddr::Impl
{
public:
    /**
     * Factory method that returns a new instance based on an IPv4 address.
     * @param[in] addr         IPv4 address in network byte order
     * @return                 New instance
     * @throws std::bad_alloc  Required memory can't be allocated
     * @exceptionsafety        Strong guarantee
     * @threadsafety           Safe
     */
    static Impl* create(const struct in_addr& addr);

    /**
     * Factory method that returns a new instance based on an IPv6 address.
     * @param[in] addr                IPv6 address
     * @return                        New instance
     * @throws std::bad_alloc         Required memory can't be allocated
     * @throws std::invalid_argument  String representation is invalid
     * @exceptionsafety               Strong guarantee
     * @threadsafety                  Safe
     */
    static Impl* create(const struct in6_addr& addr);

    /**
     * Factory method that returns a new instance based on the string
     * representation of an Internet address.
     * @param[in] addr                String representation of Internet address.
     *                                May be hostname, IPv4, or IPv6.
     * @return                        New instance
     * @throws std::bad_alloc         Required memory can't be allocated
     * @throws std::invalid_argument  String representation is invalid
     * @exceptionsafety               Strong guarantee
     * @threadsafety                  Safe
     */
    static Impl* create(const std::string addr);

    /**
     * Destructor.
     */
    virtual ~Impl() noexcept;

    /**
     * Returns the `struct sockaddr_storage` corresponding to this instance and
     * a port number.
     * @param[out] storage   Storage structure. Set upon return.
     * @param[in]  port      Port number in host byte order
     * @param[in]  sockType  Socket type hint as for `socket()`. 0 =>
     *                       unspecified. Might be ignored.
     * @exceptionsafety      Nothrow
     * @threadsafety         Safe
     */
    virtual void setSockAddrStorage(
            sockaddr_storage& storage,
            const int         port,
            const int         sockType = 0) const =0;

    /**
     * Returns the hash code of this instance.
     * @return          Instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    virtual size_t hash() const noexcept =0;

    /**
     * Indicates if this instance is considered equal to another.
     * @param[in] that  Other instance
     * @retval `true`   Iff this instance is considered equal to the other
     */
    virtual bool operator==(const Impl& that) const noexcept =0;

    /**
     * Indicates if this instance is considered equal to an IPv4 address.
     * @param[in] that  IPv4 address
     * @retval `true`   Iff this instance is considered equal to the IPv4
     *                  address
     */
    virtual bool operator==(const Ipv4Addr& that) const noexcept =0;

    /**
     * Indicates if this instance is considered equal to an IPv6 address.
     * @param[in] that  IPv6 address
     * @retval `true`   Iff this instance is considered equal to the IPv6
     *                  address
     */
    virtual bool operator==(const Ipv6Addr& that) const noexcept =0;

    /**
     * Indicates if this instance is considered equal to a hostname address.
     * @param[in] that  Hostname address
     * @retval `true`   Iff this instance is considered equal to the hostname
     *                  address
     */
    virtual bool operator==(const InetNameAddr& that) const noexcept =0;

    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  Other instance
     * @retval `true`   Iff this instance is considered less than the other
     */
    virtual bool operator<(const Impl& that) const noexcept =0;

    /**
     * Indicates if this instance is considered less than an IPv4 address.
     * @param[in] that  IPv4 address
     * @retval `true`   Iff this instance is considered less than the IPv4
     *                  address
     */
    virtual bool operator<(const Ipv4Addr& that) const noexcept =0;

    /**
     * Indicates if this instance is considered less than an IPv6 address.
     * @param[in] that  IPv6 address
     * @retval `true`   Iff this instance is considered less than the IPv6
     *                  address
     */
    virtual bool operator<(const Ipv6Addr& that) const noexcept =0;

    /**
     * Indicates if this instance is considered less than a hostname address.
     * @param[in] that  Hostname address
     * @retval `true`   Iff this instance is considered less than the hostname
     *                  address
     */
    virtual bool operator<(const InetNameAddr& that) const noexcept =0;

    /**
     * Returns the string representation of the Internet address.
     * @return The string representation of the Internet address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    virtual std::string to_string() const =0;

    /**
     * Returns a new socket.
     * @param[in] sockType  Type of socket as defined in <sys/socket.h>:
     *                        - SOCK_STREAM     Streaming socket (e.g., TCP)
     *                        - SOCK_DGRAM      Datagram socket (e.g., UDP)
     *                        - SOCK_SEQPACKET  Record-oriented socket
     * @return Corresponding new socket
     * @throws std::system_error  `socket()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    virtual int getSocket(const int sockType) const =0;

    /**
     * Sets the interface to use for outgoing datagrams.
     * @param[in] inetAddr  Internet address of interface
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    virtual void setInterface(const int sd) const =0;

    /**
     * Sets the hop-limit on a socket for outgoing multicast packets.
     * @param[in] sd     Socket
     * @param[in] limit  Hop limit:
     *                     -         0  Restricted to same host. Won't be
     *                                  output by any interface.
     *                     -         1  Restricted to the same subnet. Won't
     *                                  be forwarded by a router (default).
     *                     -    [2,31]  Restricted to the same site,
     *                                  organization, or department.
     *                     -   [32,63]  Restricted to the same region.
     *                     -  [64,127]  Restricted to the same continent.
     *                     - [128,255]  Unrestricted in scope. Global.
     * @throws std::system_error  `setsockopt()` failure
     * @execptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    virtual void setHopLimit(
            const int      sd,
            const unsigned limit) const =0;

    /**
     * Sets whether or not a multicast packet written to a socket will be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] sd      Socket descriptor
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @return            This instance
     * @exceptionsafety   Strong guarantee
     * @threadsafety      Safe
     */
    virtual void setMcastLoop(
            const int  sd,
            const bool enable) const =0;
};

InetAddr::Impl::~Impl() noexcept
{}

InetAddr::InetAddr(InetAddr::Impl* impl)
    : pImpl{impl}
{}

InetAddr::InetAddr()
    : pImpl{Impl::create("")}
{}

InetAddr::InetAddr(const struct in_addr& addr)
    : pImpl{Impl::create(addr)}
{}

InetAddr::InetAddr(const in6_addr& addr)
    : pImpl{Impl::create(addr)}
{}

InetAddr::InetAddr(const std::string& addr)
    : pImpl{Impl::create(addr)}
{}

void InetAddr::setSockAddrStorage(
        sockaddr_storage& storage,
        const int         port,
        const int         sockType) const noexcept
{
    return pImpl->setSockAddrStorage(storage, port, sockType);
}

size_t InetAddr::hash() const noexcept
{
    return pImpl->hash();
}

bool InetAddr::operator ==(const InetAddr& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

bool InetAddr::operator !=(const InetAddr& that) const noexcept
{
    return !(*this == that);
}

bool InetAddr::operator <(const InetAddr& that) const noexcept
{
    return *pImpl.get() < *that.pImpl.get();
}

bool InetAddr::operator <=(const InetAddr& that) const noexcept
{
    return (*this < that) || (*this == that);
}

bool InetAddr::operator >=(const InetAddr& that) const noexcept
{
    return (that < *this) || (*this == that);
}

bool InetAddr::operator >(const InetAddr& that) const noexcept
{
    return !((*this < that) || (*this == that));
}

std::string InetAddr::to_string() const
{
    return pImpl->to_string();
}

int InetAddr::getSocket(const int sockType) const
{
    return pImpl->getSocket(sockType);
}

void InetAddr::setInterface(const int sd) const
{
    pImpl->setInterface(sd);
}

void InetAddr::setHopLimit(
        const int      sd,
        const unsigned limit) const
{
    pImpl->setHopLimit(sd, limit);
}

void InetAddr::setMcastLoop(
        const int      sd,
        const bool     enable) const
{
    pImpl->setMcastLoop(sd, enable);
}

/******************************************************************************/

class IpAddr : public InetAddr::Impl {
public:
    virtual ~IpAddr() noexcept =0;
};

IpAddr::~IpAddr() noexcept
{}

/******************************************************************************/

class Ipv4Addr final : public IpAddr
{
    in_addr_t ipAddr;

public:
    /**
     * Constructs from an IPv4 address in network byte order.
     * @param[in] ipAddr  IPv4 address in network byte order
     */
    explicit Ipv4Addr(const in_addr_t ipAddr) noexcept
        : ipAddr{ipAddr}
    {};

    /**
     * Constructs from an IPv4 address.
     * @param[in] ipAddr  IPv4 address
     * @exceptionsafety Nothrow
     */
    explicit Ipv4Addr(const struct in_addr& ipAddr) noexcept
        : ipAddr{ipAddr.s_addr}
    {};

    ~Ipv4Addr() noexcept
    {}

    /**
     * Returns the `struct sockaddr_storage` corresponding to this instance and
     * a port number.
     * @param[out] storage   Storage structure. Set upon return.
     * @param[in]  port      Port number in host byte order
     * @param[in]  sockType  Ignored. Socket type hint as for `socket()`.
     *                       0 => unspecified.
     * @return Socket address as a `struct sockaddr_storage`
     * @exceptionsafety  Nothrow
     * @threadsafety     Safe
     */
    void setSockAddrStorage(
            sockaddr_storage& storage,
            const int         port,
            const int         sockType = 0) const noexcept
    {
        ::memset(&storage, 0, sizeof(storage));
        struct sockaddr_in* sockAddr =
                reinterpret_cast<struct sockaddr_in*>(&storage);
        sockAddr->sin_family = AF_INET;
        sockAddr->sin_port = htons(port);
        sockAddr->sin_addr.s_addr = ipAddr;
    }

    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t hash() const noexcept
    {
        std::hash<uint32_t> h;
        return h(ipAddr);
    }

    bool operator==(const InetAddr::Impl& that) const noexcept
    {
        return that == *this;
    }

    bool operator==(const Ipv4Addr& that) const noexcept
    {
        return ::ntohl(ipAddr) == ::ntohl(that.ipAddr);
    }

    bool operator==(const Ipv6Addr& that) const noexcept
    {
        return false;
    }

    bool operator==(const InetNameAddr& that) const noexcept
    {
        return false;
    }

    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  Other instance
     * @retval `true`   Iff this instance is considered less than the other
     */
    bool operator<(const InetAddr::Impl& that) const noexcept
    {
        return !((that < *this) || (that == *this));
    }

    /**
     * Indicates if this instance is considered less than an IPv4 address.
     * @param[in] that  IPv4 address
     * @retval `true`   Iff this instance is considered less than the IPv4
     *                  address
     */
    bool operator<(const Ipv4Addr& that) const noexcept
    {
        return ::ntohl(ipAddr) < ::ntohl(that.ipAddr);
    }

    /**
     * Indicates if this instance is considered less than an IPv6 address.
     * @param[in] that  IPv6 address
     * @retval `true`   Iff this instance is considered less than the IPv6
     *                  address
     */
    bool operator<(const Ipv6Addr& that) const noexcept
    {
        return true;
    }

    /**
     * Indicates if this instance is considered less than a hostname address.
     * @param[in] that  Hostname address
     * @retval `true`   Iff this instance is considered less than the hostname
     *                  address
     */
    bool operator<(const InetNameAddr& that) const noexcept
    {
        return true;
    }

    /**
     * Returns the string representation of the IPv4 address.
     * @return The string representation of the IPv4 address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const
    {
        char buf[INET_ADDRSTRLEN];
        return std::string(inet_ntop(AF_INET, &ipAddr, buf, sizeof(buf)));
    }

    /**
     * Returns a new socket.
     * @param[in] sockType  Type of socket as defined in <sys/socket.h>:
     *                        - SOCK_STREAM     Streaming socket (e.g., TCP)
     *                        - SOCK_DGRAM      Datagram socket (e.g., UDP)
     *                        - SOCK_SEQPACKET  Record-oriented socket
     * @return           Corresponding new socket
     * @throws std::system_error  `socket()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    int getSocket(const int sockType) const
    {
        int sd = ::socket(AF_INET, sockType, 0);
        if (sd == -1)
            throw std::system_error(errno, std::system_category(),
                    "socket() failure: sockType=" + std::to_string(sockType));
        return sd;
    }

    /**
     * Sets the interface to use for outgoing datagrams.
     * @param[in] inetAddr  Internet address of interface
     * @throws SystemException  Couldn't set output interface
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setInterface(const int sd) const
    {
        if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &ipAddr,
                sizeof(ipAddr)))
            throw SYSTEM_ERROR("Couldn't set output interface to " +
                    to_string());
    }

    /**
     * Sets the hop-limit on a socket for outgoing multicast packets.
     * @param[in] sd     Socket
     * @param[in] limit  Hop limit:
     *                     -         0  Restricted to same host. Won't be
     *                                  output by any interface.
     *                     -         1  Restricted to the same subnet. Won't
     *                                  be forwarded by a router (default).
     *                     -    [2,31]  Restricted to the same site,
     *                                  organization, or department.
     *                     -   [32,63]  Restricted to the same region.
     *                     -  [64,127]  Restricted to the same continent.
     *                     - [128,255]  Unrestricted in scope. Global.
     * @throws std::system_error  `setsockopt()` failure
     * @execptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setHopLimit(
            const int      sd,
            const unsigned limit) const
    {
        if (limit > 255)
            throw std::invalid_argument("Invalid hop-limit: " +
                    std::to_string(limit));
        const unsigned char value = limit;
        if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &value,
                sizeof(value)))
            throw std::system_error(errno, std::system_category(),
                    std::string("Couldn't set hop-limit for multicast packets: "
                    "sock=") + std::to_string(sd) + ", group=" + to_string() +
                    ", limit=" + std::to_string(value));
    }

    /**
     * Sets whether or not a multicast packet written to a socket will be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] sd      Socket descriptor
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @return  This instance
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setMcastLoop(
            const int  sd,
            const bool enable) const
    {
        const unsigned char value = enable;
        if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &value,
                sizeof(value)))
            throw std::system_error(errno, std::system_category(),
                    std::string("Couldn't set multicast packet looping: "
                    "sock=") + std::to_string(sd) + ", enable=" +
                    std::to_string(enable));
    }
};

/******************************************************************************/

class Ipv6Addr final : public IpAddr
{
    struct in6_addr ipAddr;

    /**
     * Returns the index of the interface that corresponds to this Internet
     * address.
     * @param[in] sd  Socket descriptor (necessary for `ioctl()` calls
     * @return Index of corresponding interface
     * @throws SystemError
     * @exceptionsafety  Strong Guarantee
     * @threadsafety     Safe
     */
    unsigned getIfaceIndex(const int sd) const
    {
        int lastlen = 0;
        for (size_t size = 1024; ; size *= 2) {
            char          buf[size];
            struct ifconf ifc;
            if (ioctl(sd, SIOCGIFCONF, &ifc) < 0) {
                if (errno != EINVAL || lastlen != 0)
                    throw SYSTEM_ERROR("Couldn't get interface configurations");
            }
            else if (ifc.ifc_len != lastlen) {
                lastlen = ifc.ifc_len;
            }
            else {
                int                 len;
                const struct ifreq* ifr;
                for (const char* ptr = buf; ptr < buf + ifc.ifc_len;
                        ptr += sizeof(ifr->ifr_name) + len) {
                    ifr = reinterpret_cast<const struct ifreq*>(ptr);
#ifdef HAVE_STRUCT_IFREQ_IF_ADDR_SA_LEN
                    len = std::max(sizeof(struct sockaddr),
                            ifr->ifr_addr.sa_len);
#else
                    switch (ifr->ifr_addr.sa_family) {
                        case AF_INET6:
                            len = sizeof(struct sockaddr_in6);
                        break;
                        case AF_INET:
                        default:
                            len = sizeof(struct sockaddr);
                    }
#endif
                    if (ifr->ifr_addr.sa_family != AF_INET6)
                        continue;
                    auto sockAddr = reinterpret_cast<const struct sockaddr_in6*>
                            (&ifr->ifr_addr);
                    if (::memcmp(&sockAddr->sin6_addr, &ipAddr, sizeof(ipAddr))
                            == 0) {
                        unsigned index = if_nametoindex(ifr->ifr_name);
                        if (index == 0)
                            throw SYSTEM_ERROR(
                                    "Couldn't convert interface name \"" +
                                    std::string(ifr->ifr_name) + "\" to index");
                        return index;
                    } // Found matching entry
                } // Interface entry loop
            } // Have interface entries
        } // Interface entries buffer-size increment loop
        throw NOT_FOUND_ERROR("Couldn't find interface entry corresponding to "
                + to_string());
    }

public:
    /**
     * Constructs from an IPv6 address.
     * @param[in] ipAddr  IPv6 address
     */
    explicit Ipv6Addr(const struct in6_addr& ipAddr)
        : ipAddr{}
    {
        ::memcpy(&this->ipAddr, &ipAddr, sizeof(ipAddr));
    }

    /**
     * Returns the `struct sockaddr_storage` corresponding to this instance and
     * a port number.
     * @param[out] storage   Storage structure. Set upon return.
     * @param[in]  port      Port number in host byte order
     * @param[in]  sockType  Ignored. Socket type hint as for `socket()`.
     *                       0 => unspecified.
     * @return Socket address as a `struct sockaddr_storage`
     * @exceptionsafety      Strong guarantee
     * @threadsafety         Safe
     */
    void setSockAddrStorage(
            sockaddr_storage& storage,
            const int         port,
            const int         sockType = 0) const
    {
        ::memset(&storage, 0, sizeof(storage));
        struct sockaddr_in6* sockAddr =
                reinterpret_cast<struct sockaddr_in6*>(&storage);
        sockAddr->sin6_family = AF_INET6;
        sockAddr->sin6_port = htons(port);
        sockAddr->sin6_addr = ipAddr;
    }

    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t hash() const noexcept
    {
        const size_t*       ptr =
                reinterpret_cast<const size_t*>(&ipAddr.s6_addr);
        const size_t* const out = ptr + sizeof(ipAddr.s6_addr)/sizeof(size_t);
        size_t              hash = 0;
        while (ptr < out)
            hash ^= *ptr++;
        return hash;
    }

    bool operator==(const InetAddr::Impl& that) const noexcept
    {
        return that == *this;
    }

    bool operator==(const Ipv4Addr& that) const noexcept
    {
        return false;
    }

    bool operator==(const Ipv6Addr& that) const noexcept
    {
        return ::memcmp(ipAddr.s6_addr, that.ipAddr.s6_addr,
                sizeof(ipAddr.s6_addr)) == 0;
    }

    bool operator==(const InetNameAddr& that) const noexcept
    {
        return false;
    }

    /**
     * Indicates if this instance is considered less than an Internet address.
     * @param[in] that  Internet address
     * @retval `true`   Iff this instance is considered less than the Internet
     *                  address
     */
    bool operator<(const InetAddr::Impl& that) const noexcept
    {
        return !((that < *this) || (that == *this));
    }

    /**
     * Indicates if this instance is considered less than an IPv4 address.
     * @param[in] that  IPv4 address
     * @retval `true`   Iff this instance is considered less than the IPv4
     *                  address
     */
    bool operator<(const Ipv4Addr& that) const noexcept
    {
        return false;
    }

    /**
     * Indicates if this instance is considered less than an IPv6 address.
     * @param[in] that  IPv6 address
     * @retval `true`   Iff this instance is considered less than the IPv6
     *                  address
     */
    bool operator<(const Ipv6Addr& that) const noexcept
    {
        return ::memcmp(ipAddr.s6_addr, that.ipAddr.s6_addr,
                sizeof(ipAddr.s6_addr)) < 0;
    }

    /**
     * Indicates if this instance is considered less than a hostname address.
     * @param[in] that  Hostname address
     * @retval `true`   Iff this instance is considered less than the hostname
     *                  address
     */
    bool operator<(const InetNameAddr& that) const noexcept
    {
        return true;
    }

    /**
     * Returns a string representation of the IPv6 address.
     * @return A string representation of the IPv6 address.
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const
    {
        char buf[INET6_ADDRSTRLEN];
        return std::string(inet_ntop(AF_INET6, &ipAddr.s6_addr, buf,
                sizeof(buf)));
    }

    /**
     * Returns a new socket.
     * @param[in] sockType  Type of socket as defined in <sys/socket.h>:
     *                        - SOCK_STREAM     Streaming socket (e.g., TCP)
     *                        - SOCK_DGRAM      Datagram socket (e.g., UDP)
     *                        - SOCK_SEQPACKET  Record-oriented socket
     * @return Corresponding new socket
     * @throws std::system_error  `socket()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    int getSocket(const int sockType) const
    {
        int sd = ::socket(AF_INET6, sockType, 0);
        if (sd == -1)
            throw std::system_error(errno, std::system_category(),
                    "socket() failure: sockType=" + std::to_string(sockType));
        return sd;
    }

    /**
     * Sets the interface to use for outgoing datagrams.
     * @param[in] inetAddr  Internet address of interface
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setInterface(const int sd) const
    {
        unsigned ifaceIndex = getIfaceIndex(sd);
        if (setsockopt(sd, IPPROTO_IP, IPV6_MULTICAST_IF, &ifaceIndex,
                sizeof(ifaceIndex)))
            throw SYSTEM_ERROR("Couldn't set output interface to " +
                    to_string());
    }

    /**
     * Sets the hop-limit on a socket for outgoing multicast packets.
     * @param[in] sd     Socket
     * @param[in] limit  Hop limit:
     *                     -         0  Restricted to same host. Won't be
     *                                  output by any interface.
     *                     -         1  Restricted to the same subnet. Won't
     *                                  be forwarded by a router (default).
     *                     -    [2,31]  Restricted to the same site,
     *                                  organization, or department.
     *                     -   [32,63]  Restricted to the same region.
     *                     -  [64,127]  Restricted to the same continent.
     *                     - [128,255]  Unrestricted in scope. Global.
     * @throws std::system_error  `setsockopt()` failure
     * @execptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setHopLimit(
            const int      sd,
            const unsigned limit) const
    {
        if (limit > 255)
            throw std::invalid_argument("Invalid hop-limit: " +
                    std::to_string(limit));
        const int value = limit;
        if (::setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &value,
                sizeof(value)))
            throw std::system_error(errno, std::system_category(),
                    std::string("Couldn't set hop-limit for multicast packets: "
                    "sock=") + std::to_string(sd) + ", group=" + to_string() +
                    ", limit=" + std::to_string(value));
    }

    /**
     * Sets whether or not a multicast packet written to a socket will be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] sd      Socket descriptor
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @return  This instance
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setMcastLoop(
            const int  sd,
            const bool enable) const
    {
        const unsigned value = enable;
        if (::setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &value,
                sizeof(value)))
            throw std::system_error(errno, std::system_category(),
                    std::string("Couldn't set multicast packet looping: "
                    "sock=") + std::to_string(sd) + ", enable=" +
                    std::to_string(enable));
    }
};

/******************************************************************************/

class InetNameAddr final : public InetAddr::Impl
{
    std::string name; /// Hostname

    /**
     * Adds Internet addresses to a set.
     * @param[in]  family         Internet address family of addresses to add
     * @param[in]  sockType       Type of socket as defined in <sys/socket.h>
     * @param[in]  port           Port number in host byte order
     * @param[out] set            Set of Internet addresses.
     * @throws std::system_error  The IP address couldn't be obtained
     * @exceptionsafety           Strong guarantee
     * @threadsafety              Safe
     */
    void getSockAddrs(
            const int                                family,
            const int                                sockType,
            const in_port_t                          port,
            std::set<struct sockaddr_storage>* const set) const
    {
        struct addrinfo  hints = {};
        hints.ai_family = family;
        hints.ai_socktype = sockType;
        hints.ai_protocol = (sockType == SOCK_STREAM)
                ? IPPROTO_SCTP
                : sockType == SOCK_DGRAM
                  ? IPPROTO_UDP
                  : 0;
        struct addrinfo* list;
        if (::getaddrinfo(name.data(), nullptr, &hints, &list))
            throw std::system_error(errno, std::system_category(),
                    std::string("::getaddrinfo() failure for host \"") +
                    name.data() + "\"");
        try {
            uint16_t netPort = htons(port);
            for (struct addrinfo* entry = list; entry != NULL;
                    entry = entry->ai_next) {
                union {
                    struct sockaddr_in      ipv4;
                    struct sockaddr_in6     ipv6;
                    struct sockaddr_storage storage;
                } sockaddr = {};
                if (entry->ai_addr->sa_family == AF_INET) {
                    sockaddr.ipv4 = *reinterpret_cast<
                            struct sockaddr_in*>(entry->ai_addr);
                    sockaddr.ipv4.sin_port = netPort;
                    set->insert(sockaddr.storage);
                }
                else if (entry->ai_addr->sa_family == AF_INET6) {
                    sockaddr.ipv6 = *reinterpret_cast<
                            struct sockaddr_in6*>(entry->ai_addr);
                    sockaddr.ipv6.sin6_port = netPort;
                    set->insert(sockaddr.storage);
                }
            }
            ::freeaddrinfo(list);
        }
        catch (const std::exception& e) {
            ::freeaddrinfo(list);
            throw;
        }
    }

    /**
     * Returns the first IP-based Internet address of the given family that's
     * associated with this instance's hostname.
     * @param[in] family  Address family. One of `AF_INET` or `AF_INET6`.
     * @retval NULL  No address found
     * @return       First matching address
     * @throws std::system_error `getaddrinfo()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    IpAddr* getIpAddr(const int family) const
    {
        struct addrinfo  hints = {};
        hints.ai_family = family; // Use first entry
        hints.ai_socktype = SOCK_DGRAM;
        struct addrinfo* list;
        if (::getaddrinfo(name.data(), nullptr, &hints, &list))
            throw SYSTEM_ERROR(
                    std::string("::getaddrinfo() failure for host \"") +
                    name.data() + "\"", errno);
        try {
            IpAddr* ipAddr = nullptr;
            for (struct addrinfo* entry = list; entry != NULL;
                    entry = entry->ai_next) {
                if (entry->ai_addr->sa_family == AF_INET) {
                    struct sockaddr_in* sockAddr = reinterpret_cast<
                            struct sockaddr_in*>(entry->ai_addr);
                    ipAddr = new Ipv4Addr(sockAddr->sin_addr.s_addr);
                    break;
                }
                else if (entry->ai_addr->sa_family == AF_INET6) {
                    struct in6_addr addr = *reinterpret_cast<
                            struct in6_addr*>(entry->ai_addr->sa_data);
                    ipAddr = new Ipv6Addr(addr);
                    break;
                }
            }
            freeaddrinfo(list);
            return ipAddr;
        }
        catch (...) {
            freeaddrinfo(list);
            throw;
        }
    }

    /**
     * Returns the first IP-based Internet address associated with this
     * instance's hostname.
     * @retval NULL  No address found
     * @return       First matching address
     * @throws std::system_error `getaddrinfo()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    IpAddr* getIpAddr() const
    {
        IpAddr* ipAddr = getIpAddr(AF_INET);
        return (ipAddr != nullptr)
                ? ipAddr
                : getIpAddr(AF_INET6);
    }

public:
    /**
     * Constructs from a hostname.
     * @param[in] name  A hostname
     */
    explicit InetNameAddr(const std::string name = "")
        : name{name}
    {}

    /**
     * Returns the `struct sockaddr_storage` corresponding to this instance and
     * a port number.
     * @param[out] storage   Storage structure. Set upon return.
     * @param[in]  port      Port number in host byte order
     * @param[in]  sockType  Socket type hint as for `socket()`. 0 =>
     *                       unspecified.
     * @throw SystemError    ::getaddrinfo() failure
     * @exceptionsafety      Strong Guarantee
     * @threadsafety         Safe
     */
    void setSockAddrStorage(
            sockaddr_storage& storage,
            const int         port,
            const int         sockType = 0) const
    {
        IpAddr* const ipAddr = getIpAddr();
        try {
            ipAddr->setSockAddrStorage(storage, port, sockType);
            delete ipAddr;
        }
        catch (...) {
            delete ipAddr;
            throw;
        }
    }

    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t hash() const noexcept
    {
        std::hash<std::string> h;
        return h(name);
    }

    bool operator==(const InetAddr::Impl& that) const noexcept
    {
        return that == *this;
    }

    bool operator==(const Ipv4Addr& that) const noexcept
    {
        return false;
    }

    bool operator==(const Ipv6Addr& that) const noexcept
    {
        return false;
    }

    bool operator==(const InetNameAddr& that) const noexcept
    {
        return name == that.name;
    }

    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  Other instance
     * @retval `true`   Iff this instance is considered less than the other
     */
    bool operator<(const InetAddr::Impl& that) const noexcept
    {
        return !((that < *this) || (that == *this));
    }

    /**
     * Indicates if this instance is considered less than an IPv4 address.
     * @param[in] that  IPv4 address
     * @retval `true`   Iff this instance is considered less than the IPv4
     *                  address
     */
    bool operator<(const Ipv4Addr& that) const noexcept
    {
        return false;
    }

    /**
     * Indicates if this instance is considered less than an IPv6 address.
     * @param[in] that  IPv6 address
     * @retval `true`   Iff this instance is considered less than the IPv6
     *                  address
     */
    bool operator<(const Ipv6Addr& that) const noexcept
    {
        return false;
    }

    /**
     * Indicates if this instance is considered less than a hostname address.
     * @param[in] that  Hostname address
     * @retval `true`   Iff this instance is considered less than the hostname
     *                  address
     */
    bool operator<(const InetNameAddr& that) const noexcept
    {
        return name < that.name;
    }

    /**
     * Returns the hostname.
     * @return The hostname
     * @exceptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    std::string to_string() const noexcept
    {
        return name;
    }

    /**
     * Returns a new socket.
     * @param[in] sockType  Type of socket as defined in <sys/socket.h>:
     *                        - SOCK_STREAM     Streaming socket (e.g., TCP)
     *                        - SOCK_DGRAM      Datagram socket (e.g., UDP)
     *                        - SOCK_SEQPACKET  Record-oriented socket
     * @return Corresponding new socket
     * @throws std::system_error  `socket()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    int getSocket(const int sockType) const
    {
        IpAddr* const ipAddr = getIpAddr();
        try {
            int sd = ipAddr->getSocket(sockType);
            delete ipAddr;
            return sd;
        }
        catch (...) {
            delete ipAddr;
            throw;
        }
    }

    /**
     * Sets the interface to use for outgoing datagrams.
     * @param[in] sd     Socket descriptor
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setInterface(const int sd) const
    {
        IpAddr* const ipAddr = getIpAddr();
        try {
            ipAddr->setInterface(sd);
            delete ipAddr;
        }
        catch (...) {
            delete ipAddr;
            throw;
        }
    }

    /**
     * Sets the hop-limit on a socket for outgoing multicast packets.
     * @param[in] sd     Socket
     * @param[in] limit  Hop limit:
     *                     -         0  Restricted to same host. Won't be
     *                                  output by any interface.
     *                     -         1  Restricted to the same subnet. Won't
     *                                  be forwarded by a router (default).
     *                     -    [2,31]  Restricted to the same site,
     *                                  organization, or department.
     *                     -   [32,63]  Restricted to the same region.
     *                     -  [64,127]  Restricted to the same continent.
     *                     - [128,255]  Unrestricted in scope. Global.
     * @throws std::system_error  `setsockopt()` failure
     * @execptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setHopLimit(
            const int      sd,
            const unsigned limit) const
    {
        IpAddr* const ipAddr = getIpAddr();
        try {
            ipAddr->setHopLimit(sd, limit);
            delete ipAddr;
        }
        catch (...) {
            delete ipAddr;
            throw;
        }
    }

    /**
     * Sets whether or not a multicast packet written to a socket will be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] sd      Socket descriptor
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @return  This instance
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setMcastLoop(
            const int  sd,
            const bool enable) const
    {
        IpAddr* const ipAddr = getIpAddr();
        try {
            ipAddr->setMcastLoop(sd, enable);
            delete ipAddr;
        }
        catch (...) {
            delete ipAddr;
            throw;
        }
    }
};

/******************************************************************************/

InetAddr::Impl* InetAddr::Impl::create(const struct in_addr& addr)
{
    return new Ipv4Addr(addr);
}

InetAddr::Impl* InetAddr::Impl::create(const struct in6_addr& addr)
{
    return new Ipv6Addr(addr);
}

InetAddr::Impl* InetAddr::Impl::create(const std::string addr)
{
    struct in_addr ipv4_addr;
    if (inet_pton(AF_INET, addr.data(), &ipv4_addr) == 1) {
        return new Ipv4Addr(ipv4_addr);
    }
    else {
        struct in6_addr ipv6_addr;
        if (inet_pton(AF_INET6, addr.data(), &ipv6_addr) == 1) {
            return new Ipv6Addr(ipv6_addr);
        }
        else {
            return new InetNameAddr(addr);
        }
    }
}

} // namespace
