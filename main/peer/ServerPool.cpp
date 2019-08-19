/**
 * 
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: HostFactory.cpp
 *  Created on: Jun 29, 2019
 *      Author: Steven R. Emmerson
 */

#include <ServerPool.h>
#include "config.h"

#include "DelayQueue.h"

namespace hycast {

class ServerPool::Impl {
public:
    virtual ~Impl() =0;

    virtual bool ready() const noexcept =0;

    virtual SockAddr pop() =0;

    virtual void consider(
            const SockAddr& server,
            const unsigned  delay) =0;
};

ServerPool::Impl::~Impl()
{}

/******************************************************************************/

class ServerQueue final : public ServerPool::Impl
{
private:
    DelayQueue<SockAddr, std::chrono::seconds> servers;

public:
    ServerQueue(const std::set<SockAddr> servers)
        : servers()
    {
        for (const auto sockAddr : servers)
            this->servers.push(sockAddr); // No delay
    }

    bool ready() const noexcept
    {
        return servers.ready();
    }

    SockAddr pop()
    {
        return servers.pop();
    }

    void consider(
            const SockAddr& server,
            const unsigned  delay)
    {
        servers.push(server, delay);
    }
};

/******************************************************************************/

ServerPool::ServerPool()
    : pImpl{}
{}

ServerPool::ServerPool(const std::set<SockAddr>& servers)
    : pImpl{new ServerQueue(servers)}
{}

bool ServerPool::ready() const noexcept
{
    return pImpl->ready();
}

SockAddr ServerPool::pop() const noexcept
{
    return pImpl->pop();
}

void ServerPool::consider(
        const SockAddr& server,
        const unsigned  delay) const
{
    pImpl->consider(server, delay);
}

} // namespace
