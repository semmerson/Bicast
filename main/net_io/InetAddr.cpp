/**
 * Internet address. Could be IPv4 or IPv6.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: InAddr.cpp
 *  Created on: May 6, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "InetAddr.h"
#include "SockAddr.h"
#include "error.h"

#include <arpa/inet.h>
#include <cstring>
#include <functional>
#include <netinet/in.h>

namespace hycast {

class Inet4Addr;
class Inet6Addr;
class NameAddr;

class InetAddr::Impl
{
public:
    virtual ~Impl() noexcept;

    virtual std::string to_string() const =0;

    virtual bool operator <(const Impl& rhs) const noexcept =0;

    virtual bool operator <(const Inet4Addr& rhs) const noexcept =0;

    virtual bool operator <(const Inet6Addr& rhs) const noexcept =0;

    virtual bool operator <(const NameAddr& rhs) const noexcept =0;

    virtual bool operator ==(const Impl& rhs) const noexcept =0;

    virtual bool operator ==(const Inet4Addr& rhs) const noexcept =0;

    virtual bool operator ==(const Inet6Addr& rhs) const noexcept =0;

    virtual bool operator ==(const NameAddr& rhs) const noexcept =0;

    virtual size_t hash() const noexcept =0;

    virtual SockAddr getSockAddr(const in_port_t port) const =0;

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
        // NB: The following is independent of protocol (i.e., IPv4 or IPv6)
        struct group_source_req mreq;

        mreq.gsr_interface = 0; // => O/S chooses interface
                getSockAddr(0).setAddr(mreq.gsr_group);
        srcAddr.getSockAddr(0).setAddr(mreq.gsr_source);

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
    Inet4Addr(const in_addr_t addr)
        : addr{addr}
    {}

    std::string to_string() const
    {
        char buf[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET, &addr.s_addr, buf, sizeof(buf)) == nullptr)
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

    SockAddr getSockAddr(const in_port_t port) const
    {
        return SockAddr(addr.s_addr, port);
    }
};

/******************************************************************************/

class Inet6Addr final : public InetAddr::Impl
{
    struct in6_addr     addr;
    std::hash<uint64_t> myHash;

public:
    Inet6Addr(const struct in6_addr& addr)
        : addr(addr)
    {}

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
    {;
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
        return myHash(((static_cast<uint64_t>(addr.s6_addr32[0]) ^
                addr.s6_addr32[1]) << 32) |
                (addr.s6_addr32[2] ^ addr.s6_addr32[3]));
    }

    SockAddr getSockAddr(const in_port_t port) const
    {
        return SockAddr(addr, port);
    }
};

/******************************************************************************/

class NameAddr final : public InetAddr::Impl
{
    std::string            name;
    std::hash<std::string> myHash;

public:
    NameAddr(const std::string& name)
        : name{name}
    {}

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

    SockAddr getSockAddr(const in_port_t port) const
    {
        return SockAddr(name, port);
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
    : pImpl(new Inet4Addr(addr.s_addr))
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

std::string InetAddr::to_string() const
{
    return pImpl->to_string();
}

bool InetAddr::operator <(const InetAddr& rhs) const noexcept
{
    return pImpl->operator <(*rhs.pImpl.get());
}

bool InetAddr::operator ==(const InetAddr& rhs) const noexcept
{
    return pImpl->operator ==(*rhs.pImpl.get());
}

size_t InetAddr::hash() const noexcept
{
    return pImpl->hash();
}

SockAddr InetAddr::getSockAddr(const in_port_t port) const
{
    return pImpl->getSockAddr(port);
}

void InetAddr::join(
        const int       sd,
        const InetAddr& srcAddr) const
{
    return pImpl->join(sd, srcAddr);
}

} // namespace
