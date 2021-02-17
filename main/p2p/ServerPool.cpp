/**
 * A pool of remote servers for creating remote peers.
 *
 *        File: ServerPool.cpp
 *  Created on: Jun 29, 2019
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

#include "ServerPool.h"

#include "DelayQueue.h"
#include "LinkedMap.h"

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

    virtual void consider(SockAddr& server) =0;

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
    const unsigned                             delay;

public:
    ServerQueue()
        : servers()
        , delay(0)
    {}

    ServerQueue(
            const std::set<SockAddr>& servers,
            const unsigned            delay)
        : servers()
        , delay{delay}
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
        return servers.pop();
    }

    void consider(SockAddr& server) override
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
#if 0
/**
 * Thread-safe set of server addresses.
 */
class ServerSet final : public ServerPool::Impl
{
private:
    using Mutex      = std::mutex;
    using Guard      = std::lock_guard<Mutex>;
    using Lock       = std::unique_lock<Mutex>;
    using Cond       = std::condition_variable;
    using Index      = unsigned;

    mutable Mutex              mutex;
    mutable Cond               cond;
    LinkedMap<Index, SockAddr> servers;
    const Index                maxServers;
    Index                      nextIndex;
    bool                       closed;

public:
    /**
     * @param[in] maxServers   Maximum number of servers to track
     * @throw InvalidArgument  `maxServers == 0`
     * @throw InvalidArgument  `maxServers` is too large
     */
    ServerSet(const Index maxServers)
        : mutex{}
        , cond{}
        , servers(maxServers)
        , maxServers(maxServers)
        , nextIndex(0)
        , closed{false}
    {
        if (maxServers == 0)
            throw INVALID_ARGUMENT("Maximum number of servers is zero");
    }

    void consider(SockAddr& server) override {
        Guard guard{mutex};

        if (servers.add(nextIndex, server).second) {
            ++nextIndex;

            while (servers.size() > maxServers)
                servers.pop();

            cond.notify_all();
        }
    }

    bool ready() const noexcept override {
        Guard guard{mutex};
        return !servers.empty();
    }

    /**
     * @exceptionsafety   Strong guarantee
     * @cancellationpoint
     */
    SockAddr pop() override {
        Lock lock{mutex};

        while (!closed && servers.empty())
            cond.wait(lock);

        if (closed)
            throw DOMAIN_ERROR("ServerSet is closed");

        return servers.pop();
    }

    void close() override {
        Guard guard{mutex};
        closed = true;
        cond.notify_all();
    }

    bool empty() const override {
        Guard guard{mutex};
        return servers.empty();
    }
};
#endif

/******************************************************************************/

ServerPool::ServerPool()
    : pImpl{std::make_shared<ServerQueue>()} {
}

ServerPool::ServerPool(
        const std::set<SockAddr>& servers,
        const unsigned            delay)
    : pImpl{std::make_shared<ServerQueue>(servers, delay)} {
}

//ServerPool::ServerPool(const unsigned maxServers)
    //: pImpl{std::make_shared<ServerSet>(maxServers)} {
//}

bool ServerPool::ready() const noexcept {
    return pImpl->ready();
}

SockAddr ServerPool::pop() const {
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

void ServerPool::consider(SockAddr& server) const {
    pImpl->consider(server);
}

void ServerPool::close() {
    pImpl->close();
}

bool ServerPool::empty() const {
    return pImpl->empty();
}

} // namespace
