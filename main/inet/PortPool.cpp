/**
 * A thread-safe pool of port-numbers.
 *
 *        File: PortPool.cpp
 *  Created on: Jul 13, 2019
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

    mutable Mutex   mutex;
    mutable Cond    cond;
    Queue           queue;
    const in_port_t min;
    const unsigned  num;

public:
    Impl() = default;

    Impl(   const in_port_t min,
            const unsigned  num)
        : mutex()
        , cond()
        , queue{}
        , min{min}
        , num{num}
    {
        for (in_port_t port = min, end = min + num; port != end; ++port)
            queue.emplace(port);
    }

    in_port_t getMin() const noexcept {
        return min;
    }

    unsigned getNum() const noexcept {
        return num;
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

in_port_t PortPool::getMin() const noexcept {
    return pImpl->getMin();
}

unsigned PortPool::getNum() const noexcept {
    return pImpl->getNum();
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
