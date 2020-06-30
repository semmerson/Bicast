/**
 * A thread-safe pool of port-numbers.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PortPool.cpp
 *  Created on: Jul 13, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "PortPool.h"

#include <condition_variable>
#include <list>
#include <mutex>
#include <queue>

namespace hycast {

class PortPool::Impl
{
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;
    using Lock = std::unique_lock<Mutex>;
    using Cond = std::condition_variable;
    using Queue = std::queue<in_port_t, std::list<in_port_t>>;

    mutable Mutex mutex;
    mutable Cond  cond;
    Queue         queue;

public:
    Impl() = default;

    Impl(   const in_port_t min,
            const unsigned  num)
        : mutex()
        , cond()
        , queue{}
    {
        for (in_port_t port = min, end = min + num; port != end; ++port)
            queue.emplace(port);
    }

    int size() const {
        Guard guard(mutex);
        return queue.size();
    }

    in_port_t take() {
        Lock lock(mutex);

        while (queue.empty())
            cond.wait(lock);

        auto port = queue.front();
        queue.pop();

        LOG_DEBUG("Returning port %u", port);
        return port;
    }

    void add(const in_port_t port) {
        Guard guard(mutex);

        //LOG_DEBUG("Adding port %u", port);
        queue.emplace(port);
        cond.notify_one();
    }
};

/******************************************************************************/

PortPool::PortPool()
    : pImpl{} {
}

PortPool::PortPool(
        const in_port_t min,
        const unsigned  num)
    : pImpl{new Impl(min, num)} {
}

PortPool::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

int PortPool::size() const {
    return pImpl->size();
}

in_port_t PortPool::take() const {
    return pImpl->take();
}

void PortPool::add(in_port_t port) const {
    return pImpl->add(port);
}

} // namespace
