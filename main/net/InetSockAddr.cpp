/**
 * This file defines an Internet socket address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetSockAddr.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"
#include "InetAddr.h"
#include "InetSockAddr.h"
#include "PortNumber.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <functional>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>

namespace hycast {

class InetSockAddrImpl final
{
    InetAddr  inetAddr; /// IP address
    in_port_t port;     /// Port number in host byte-order

    /**
     * Returns the type of a socket.
     * @param[in] sd  Socket descriptor
     * @return        Type of socket. One of `SOCK_STREAM` or `SOCK_DGRAM`.
     * @throws std::system_error  `getsockopt()` failed
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    static int getSockType(const int sd)
    {
        int       sockType;
        socklen_t len = sizeof(sockType);
        int       status = ::getsockopt(sd, SOL_SOCKET, SO_TYPE, &sockType,
                &len);
        if (status)
            throw std::system_error(errno, std::system_category(),
                    "getsockopt() failure");
        return sockType;
    }

    /**
     * Returns the protocol level associated with an address family. This
     * function is useful for determining the second argument of `setsockopt()`.
     * @param[in] family  Address family
     * @return            Associated protocol level
     * @throws std::invalid_argument  The family is unknown
     * @threadsafety  Safe
     */
    static int familyToLevel(const sa_family_t family)
    {
        switch (family) {
            case AF_INET:
                return IPPROTO_IP;
            case AF_INET6:
                return IPPROTO_IPV6;
            default:
                throw std::invalid_argument(
                        std::string("Unknown address family: ") +
                        std::to_string(family));
        }
    }

public:
    /**
     * Constructs from nothing. The resulting instance will have the default
     * Internet address ("localhost") and the port number will be 0.
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl()
        : inetAddr(),
          port(0)
    {}

    /**
     * Constructs from an Internet address and a port number.
     * @param[in] inetAddr Internet address
     * @param[in] port     Port number
     * @throws std::bad_alloc if necessary memory can't be allocated
     * @exceptionsafety Strong
     */
    InetSockAddrImpl(
            const InetAddr   inetAddr,
            const in_port_t  port)
        : inetAddr(inetAddr)
        , port(port)
    {}

    /**
     * Constructs from a string representation of an Internet address and a port
     * number.
     * @param[in] ipAddr  String representation of Internet address
     * @param[in] port    Port number in host byte-order
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if the string representation is invalid
     */
    InetSockAddrImpl(
            const std::string ipAddr,
            const in_port_t   port)
        : inetAddr(ipAddr),
          port(port)
    {}

    /**
     * Constructs from an IPV4 address and a port number.
     * @param[in] addr  IPv4 address
     * @param[in] port  Port number
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl(
            const in_addr_t  addr,
            const PortNumber port)
        : inetAddr{addr},
          port{port.get_host()}
    {}

    /**
     * Constructs from an IPV6 address and a port number.
     * @param[in] addr  IPv6 address
     * @param[in] port  Port number in host byte-order
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl(
            const struct in6_addr& addr,
            const in_port_t        port)
        : inetAddr(addr),
          port(port)
    {}

    /**
     * Constructs from an IPv4 socket address.
     * @param[in] addr  IPv4 socket address
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl(const struct sockaddr_in& addr)
        : inetAddr(addr.sin_addr.s_addr),
          port(ntohs(addr.sin_port))
    {}

    /**
     * Constructs from an IPv6 socket address.
     * @param[in] addr  IPv6 socket address
     * @throws std::bad_alloc if required memory can't be allocated
     */
    InetSockAddrImpl(const struct sockaddr_in6& sockaddr)
        : inetAddr(sockaddr.sin6_addr),
          port(ntohs(sockaddr.sin6_port))
    {}

    /**
     * Constructs from a generic socket address.
     * @param[in] addr                Generic socket address. Must be either
     *                                IPv4 or IPv6
     * @throws std::invalid_argument  `addr` is neither IPv4 nor IPv6
     */
    InetSockAddrImpl(const struct sockaddr& sockaddr)
        : InetSockAddrImpl()
    {
        if (sockaddr.sa_family == AF_INET) {
            const struct sockaddr_in* addr =
                    reinterpret_cast<const struct sockaddr_in*>(&sockaddr);
            inetAddr = std::move(InetAddr(addr->sin_addr.s_addr));
            port = ntohs(addr->sin_port);
        }
        else if (sockaddr.sa_family == AF_INET6) {
            const struct sockaddr_in6* addr =
                    reinterpret_cast<const struct sockaddr_in6*>(&sockaddr);
            inetAddr = std::move(InetAddr(addr->sin6_addr));
            port = ntohs(addr->sin6_port);
        }
        else {
            throw std::invalid_argument("Socket address neither IPv4 nor IPv6: "
                    "sa_family=" + std::to_string(sockaddr.sa_family));
        }
    }

    /**
     * Returns the associated Internet address.
     * @return The associated Internet address
     */
    InetAddr getInetAddr() const noexcept
    {
        return inetAddr;
    }

    /**
     * Sets a socket address storage structure.
     * @param[in]     sd       Socket descriptor
     * @param[in,out] storage  Structure to be set
     */
    void setSockAddrStorage(
            const int                sd,
            struct sockaddr_storage& storage) const
    {
        int sockType = getSockType(sd);
        inetAddr.setSockAddrStorage(storage, port, sockType);
    }

    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t hash() const noexcept
    {
        return inetAddr.hash() ^ std::hash<uint16_t>()(port);
    }

    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  Other instance
     * @retval `true`   Iff this instance is less than the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator<(const InetSockAddrImpl& that) const noexcept
    {
        return (inetAddr < that.inetAddr)
                ? true
                : inetAddr == that.inetAddr && port < that.port;
    }

    /**
     * Returns the string representation of the Internet socket address.
     * @return String representation of the Internet socket address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const
    {
        std::string addr = inetAddr.to_string();
        return (addr.find(':') == std::string::npos)
                ? addr + ":" + std::to_string(port)
                : std::string("[") + addr + "]:" + std::to_string(port);
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
        return inetAddr.getSocket(sockType);
    }

    /**
     * Connects a socket's remote endpoint to this instance.
     * @param[in] sd        Socket descriptor
     * @throws std::system_error
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    const InetSockAddrImpl& connect(const int sd) const
    {
        struct sockaddr_storage storage;
        setSockAddrStorage(sd, storage);
        int status = ::connect(sd, reinterpret_cast<struct sockaddr*>(&storage),
                sizeof(storage));
        if (status)
            throw SystemError(__FILE__, __LINE__, "connect() failure: sd=" +
					std::to_string(sd) + ", sockAddr=" + to_string());
        return *this;
    }

    /**
     * Binds a socket's local endpoint to this instance.
     * @param[in] sd  Socket descriptor
     * @return        This instance
     * @throws std::system_error  `getsockopt()` failed
     * @throws std::system_error  `bind()` failed
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    const InetSockAddrImpl& bind(const int sd) const
    {
        struct sockaddr_storage storage;
        setSockAddrStorage(sd, storage);
        int status = ::bind(sd, reinterpret_cast<struct sockaddr*>(&storage),
                sizeof(storage));
        if (status)
            throw SystemError(__FILE__, __LINE__, "bind() failure: sd=" +
					std::to_string(sd) + ", sockAddr=" + to_string());
        return *this;
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
     * @returns  This instance
     * @throws std::system_error  `setsockopt()` failure
     * @execptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    const InetSockAddrImpl& setHopLimit(
            const int      sd,
            const unsigned limit) const
    {
        inetAddr.setHopLimit(sd, limit);
        return *this;
    }

    /**
     * Sets whether or not a multicast packet sent to a socket will also be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] sd      Socket descriptor
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @return  This instance
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    const InetSockAddrImpl& setMcastLoop(
            const int  sd,
            const bool enable) const
    {
        inetAddr.setMcastLoop(sd, enable);
        return *this;
    }

    /**
     * Joins a socket to the multicast group corresponding to this instance.
     * @param[in] sd  Socket descriptor
     * @return        This instance
     * @throws std::system_error  `setsockopt()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    const InetSockAddrImpl& joinMcastGroup(const int sd) const
    {
        struct group_req req;
        setSockAddrStorage(sd, req.gr_group);
        req.gr_interface = 0; // Use default multicast interface
        int level = familyToLevel(req.gr_group.ss_family);
        if (::setsockopt(sd, level, MCAST_JOIN_GROUP, &req, sizeof(req)))
            throw SystemError(__FILE__, __LINE__,
                    std::string("Couldn't join multicast group: sock=") +
                    std::to_string(sd) + ", group=" + to_string());
        return *this;
    }

    /**
     * Joins a socket to the source-specific multicast group corresponding to
     * this instance and the IP address of the source.
     * @param[in] sd       Socket descriptor
     * @param[in] srcAddr  IP address of source
     * @return             This instance
     * @throws std::system_error  `setsockopt()` failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    const InetSockAddrImpl& joinSourceGroup(
            const int       sd,
            const InetAddr& srcAddr) const
    {
        struct group_source_req req;
        setSockAddrStorage(sd, req.gsr_group);
        req.gsr_interface = 0; // Let kernel choose multicast interface
        int sockType = getSockType(sd);
        srcAddr.setSockAddrStorage(req.gsr_source, port, sockType);
        int level = familyToLevel(req.gsr_group.ss_family);
        if (::setsockopt(sd, level, MCAST_JOIN_SOURCE_GROUP, &req, sizeof(req)))
            throw SystemError(__FILE__, __LINE__,
                    std::string("Couldn't join source-specific multicast group: "
                    "sock=") + std::to_string(sd) + ", group=" + to_string() +
                    ", source=" + srcAddr.to_string());
        return *this;
    }
};

/**
 * Indicates if an instance is equal to another.
 * @param[in] that  Other instance
 * @retval `true`   Iff this instance is considered equal to the other
 * @exceptionsafety Nothrow
 * @threadsafety    Safe
 */
bool operator==(
       const InetSockAddrImpl& o1,
        const InetSockAddrImpl& o2) noexcept
{
    return !(o1 < o2) && !(o2 < o1);
}


InetSockAddr::InetSockAddr()
    : pImpl{}
{}

InetSockAddr::InetSockAddr(
        const InetAddr  inetAddr,
        const in_port_t port)
    : pImpl{new InetSockAddrImpl(inetAddr, port)}
{}

InetSockAddr::InetSockAddr(
        const std::string ip_addr,
        const in_port_t   port)
    : pImpl(new InetSockAddrImpl(ip_addr, port))
{}

InetSockAddr::InetSockAddr(
        const in_addr_t  addr,
        const PortNumber port)
    : pImpl{new InetSockAddrImpl(addr, port)}
{}

InetSockAddr::InetSockAddr(
        const struct in6_addr& addr,
        const in_port_t        port)
    : pImpl{new InetSockAddrImpl(addr, port)}
{}

InetSockAddr::InetSockAddr(const struct sockaddr_in& addr)
    : pImpl{new InetSockAddrImpl(addr)}
{}

InetSockAddr::InetSockAddr(const struct sockaddr_in6& sockaddr)
    : pImpl{new InetSockAddrImpl(sockaddr)}
{}

InetSockAddr::InetSockAddr(const struct sockaddr& sockaddr)
    : pImpl{new InetSockAddrImpl(sockaddr)}
{}

InetSockAddr::InetSockAddr(const InetSockAddr& that) noexcept
    : pImpl(that.pImpl)
{}

InetSockAddr::~InetSockAddr()
{}

InetSockAddr::operator bool() const noexcept
{
    return pImpl.operator bool();
}

InetAddr InetSockAddr::getInetAddr() const noexcept
{
    return pImpl->getInetAddr();
}

void InetSockAddr::setSockAddrStorage(
        const int                sd,
        struct sockaddr_storage& storage) const
{
    pImpl->setSockAddrStorage(sd, storage);
}

InetSockAddr& InetSockAddr::operator =(const InetSockAddr& rhs) noexcept
{
    pImpl = rhs.pImpl; // InetSockAddrImpl is an immutable class
    return *this;
}

std::string InetSockAddr::to_string() const
{
    return pImpl->to_string();
}

int InetSockAddr::getSocket(const int sockType) const
{
    return pImpl->getSocket(sockType);
}

const InetSockAddr& InetSockAddr::connect(int sd) const
{
    pImpl->connect(sd);
    return *this;
}

size_t InetSockAddr::hash() const noexcept
{
    return pImpl ? pImpl->hash() : 0;
}

bool InetSockAddr::operator<(const InetSockAddr& that) const noexcept
{
    return pImpl.get() < that.pImpl.get();
}

const InetSockAddr& InetSockAddr::bind(int sd) const
{
    pImpl->bind(sd);
    return *this;
}

const InetSockAddr& InetSockAddr::setHopLimit(
        const int      sd,
        const unsigned limit) const
{
    pImpl->setHopLimit(sd, limit);
    return *this;
}

const InetSockAddr& InetSockAddr::setMcastLoop(
        const int  sd,
        const bool enable) const
{
    pImpl->setMcastLoop(sd, enable);
    return *this;
}

const InetSockAddr& InetSockAddr::joinMcastGroup(const int sd) const
{
    pImpl->joinMcastGroup(sd);
    return *this;
}

const InetSockAddr& InetSockAddr::joinSourceGroup(
        const int       sd,
        const InetAddr& srcAddr) const
{
    pImpl->joinSourceGroup(sd, srcAddr);
    return *this;
}

} // namespace
