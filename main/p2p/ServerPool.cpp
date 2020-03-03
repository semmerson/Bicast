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

    /**
     * @exceptionsafety   Strong guarantee
     * @cancellationpoint
     */
    virtual SockAddr pop() =0;

    virtual void consider(
            const SockAddr& server,
            const unsigned  delay) =0;

    virtual void close() =0;

    virtual bool empty() const =0;
};

ServerPool::Impl::~Impl()
{}

/******************************************************************************/

class ServerQueue final : public ServerPool::Impl
{
private:
    DelayQueue<SockAddr, std::chrono::seconds> servers;

public:
    ServerQueue()
        : servers()
    {}

    ServerQueue(const std::set<SockAddr>& servers)
        : servers()
    {
        for (const SockAddr sockAddr : servers)
            this->servers.push(sockAddr); // No delay
    }

    bool ready() const noexcept override
    {
        return servers.ready();
    }

    /**
     * @exceptionsafety   Strong guarantee
     * @cancellationpoint
     */
    SockAddr pop() override
    {
        try {
            //LOG_DEBUG("servers.ready(): %d", servers.ready());
            return servers.pop();
        }
        catch (const std::exception& ex) {
            //LOG_DEBUG("Caught std::exception");
            throw;
        }
        catch (...) {
            //LOG_DEBUG("Caught ... exception");
            throw;
        }
    }

    void consider(
            const SockAddr& server,
            const unsigned  delay) override
    {
        servers.push(server, delay);
    }

    void close() override {
        servers.close();
    }

    bool empty() const override
    {
        return  servers.empty();
    }
};

/******************************************************************************/

ServerPool::ServerPool()
    : pImpl{new ServerQueue()}
{}

ServerPool::ServerPool(const std::set<SockAddr>& servers)
    : pImpl{new ServerQueue(servers)}
{}

bool ServerPool::ready() const noexcept
{
    return pImpl->ready();
}

SockAddr ServerPool::pop() const
{
    try {
        return pImpl->pop();
    }
    catch (const std::exception& ex) {
        //LOG_DEBUG("Caught std::exception");
        throw;
    }
    catch (...) {
        //LOG_DEBUG("Caught ... exception");
        throw;
    }
}

void ServerPool::consider(
        const SockAddr& server,
        const unsigned  delay) const
{
    pImpl->consider(server, delay);
}

void ServerPool::close()
{
    pImpl->close();
}

bool ServerPool::empty() const
{
    return pImpl->empty();
}

} // namespace
