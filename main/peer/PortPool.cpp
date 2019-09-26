/**
 * A pool of port-numbers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PortQueue.cpp
 *  Created on: Jul 13, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "PortPool.h"

#include <list>
#include <queue>

namespace hycast {

class PortPool::Impl {
    std::queue<in_port_t, std::list<in_port_t>> queue;

public:
    Impl() = default;

    Impl(   const in_port_t min,
            const unsigned  num)
        : queue{}
    {
        for (in_port_t port = min, end = min + num; port != end; ++port)
            queue.emplace(port);
    }

    int size() const
    {
        return queue.size();
    }

    in_port_t take()
    {
        if (queue.empty())
            throw std::range_error("PortPool is empty");

        auto port = queue.front();
        queue.pop();

        LOG_DEBUG("Returning port %u", port);
        return port;
    }

    void add(const in_port_t port)
    {
        LOG_DEBUG("Adding port %u", port);
        queue.emplace(port);
    }
};

/******************************************************************************/

PortPool::PortPool()
    : pImpl{}
{}

PortPool::PortPool(
        const in_port_t min,
        const unsigned  num)
    : pImpl{new Impl(min, num)}
{}

int PortPool::size() const
{
    return pImpl->size();
}

in_port_t PortPool::take()
{
    return pImpl->take();
}

void PortPool::add(in_port_t port)
{
    return pImpl->add(port);
}

} // namespace
