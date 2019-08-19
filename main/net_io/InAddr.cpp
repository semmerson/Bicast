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
#include "InAddr.h"
#include "SockAddr.h"
#include "error.h"

#include <arpa/inet.h>
#include <cstring>

namespace hycast {

class In4Addr;
class In6Addr;
class NameAddr;

class InAddr::Impl
{
public:
    virtual ~Impl() noexcept =0;

    virtual int getFamily() const noexcept =0;

    virtual std::string to_string() const =0;

    virtual bool operator <(const Impl& rhs) const noexcept =0;

    virtual bool operator <(const In4Addr& rhs) const noexcept =0;

    virtual bool operator <(const In6Addr& rhs) const noexcept =0;

    virtual bool operator <(const NameAddr& rhs) const noexcept =0;

    virtual bool operator ==(const Impl& rhs) const noexcept =0;

    virtual bool operator ==(const In4Addr& rhs) const noexcept =0;

    virtual bool operator ==(const In6Addr& rhs) const noexcept =0;

    virtual bool operator ==(const NameAddr& rhs) const noexcept =0;

    virtual SockAddr getSockAddr(const in_port_t port) const =0;
};

InAddr::Impl::~Impl() noexcept
{}

/******************************************************************************/

class In4Addr final : public InAddr::Impl
{
    struct in_addr      addr;

public:
    In4Addr(const in_addr_t addr)
        : addr{addr}
    {}

    int getFamily() const noexcept
    {
        return AF_INET;
    }

    std::string to_string() const
    {
        char buf[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET, &addr.s_addr, buf, sizeof(buf)) == nullptr)
            throw SYSTEM_ERROR("inet_ntop() failure");

        return std::string(buf);
    }

    bool operator <(const InAddr::Impl& rhs) const noexcept
    {
        return !(rhs < *this) && !(rhs == *this);
    }

    bool operator <(const In4Addr& rhs) const noexcept
    {
        return ntohl(addr.s_addr) < ntohl(rhs.addr.s_addr);
    }

    bool operator <(const In6Addr& rhs) const noexcept
    {
        return true;
    }

    bool operator <(const NameAddr& rhs) const noexcept
    {
        return true;
    }

    bool operator ==(const InAddr::Impl& rhs) const noexcept
    {
        return rhs == *this;
    }

    bool operator ==(const In4Addr& rhs) const noexcept
    {
        return addr.s_addr == rhs.addr.s_addr;
    }

    bool operator ==(const In6Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const NameAddr& rhs) const noexcept
    {
        return false;
    }

    SockAddr getSockAddr(const in_port_t port) const
    {
        return SockAddrIn(addr.s_addr, port);
    }
};

/******************************************************************************/

class In6Addr final : public InAddr::Impl
{
    struct in6_addr     addr;

public:
    In6Addr(const struct in6_addr& addr)
        : addr(addr)
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

    bool operator <(const InAddr::Impl& rhs) const noexcept
    {
        return !(rhs < *this) && !(rhs == *this);
    }

    bool operator <(const In4Addr& rhs) const noexcept
    {;
        return false;
    }

    bool operator <(const In6Addr& rhs) const noexcept
    {
        return ::memcmp(&addr, &rhs.addr, sizeof(addr)) < 0;
    }

    bool operator <(const NameAddr& rhs) const noexcept
    {
        return true;
    }

    bool operator ==(const InAddr::Impl& rhs) const noexcept
    {
        return rhs == *this;
    }

    bool operator ==(const In4Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const In6Addr& rhs) const noexcept
    {
        return ::memcmp(&addr, &rhs.addr, sizeof(addr)) == 0;
    }

    bool operator ==(const NameAddr& rhs) const noexcept
    {
        return false;
    }

    SockAddr getSockAddr(const in_port_t port) const
    {
        return SockAddrIn6(addr, port);
    }
};

/******************************************************************************/

class NameAddr final : public InAddr::Impl
{
    std::string name;

public:
    NameAddr(const std::string& name)
        : name{name}
    {}

    int getFamily() const noexcept
    {
        return AF_UNSPEC;
    }

    std::string to_string() const
    {
        return std::string(name);
    }

    bool operator <(const InAddr::Impl& rhs) const noexcept
    {
        return !(rhs < *this) && !(rhs == *this);
    }

    bool operator <(const In4Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator <(const In6Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator <(const NameAddr& rhs) const noexcept
    {
        return name < rhs.name;
    }

    bool operator ==(const InAddr::Impl& rhs) const noexcept
    {
        return rhs == *this;
    }

    bool operator ==(const In4Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const In6Addr& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const NameAddr& rhs) const noexcept
    {
        return name == rhs.name;
    }

    SockAddr getSockAddr(const in_port_t port) const
    {
        return SockAddrName(name, port);
    }
};

/******************************************************************************/

InAddr::InAddr() noexcept
    : pImpl()
{}

InAddr::InAddr(const in_addr_t addr) noexcept
    : pImpl(new In4Addr(addr))
{}

InAddr::InAddr(const struct in_addr& addr) noexcept
    : pImpl(new In4Addr(addr.s_addr))
{}

InAddr::InAddr(const struct in6_addr& addr) noexcept
    : pImpl(new In6Addr(addr))
{}

InAddr::InAddr(const std::string& addr)
    : pImpl()
{
    const char*     cstr = addr.data();
    struct in_addr  in4addr;
    struct in6_addr in6addr;

    if (::inet_pton(AF_INET, cstr, &in4addr) == 1) {
        pImpl.reset(new In4Addr(in4addr.s_addr));
    }
    else if (::inet_pton(AF_INET6, cstr, &in6addr) == 1) {
        pImpl.reset(new In6Addr(in6addr));
    }
    else {
        pImpl.reset(new NameAddr(addr));
    }
}

std::string InAddr::to_string() const
{
    return pImpl->to_string();
}

bool InAddr::operator <(const InAddr& rhs) const noexcept
{
    return pImpl->operator <(*rhs.pImpl.get());
}

SockAddr InAddr::getSockAddr(const in_port_t port) const
{
    return pImpl->getSockAddr(port);
}

} // namespace
