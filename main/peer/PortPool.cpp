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

#include "PortPool.h"

#include <list>
#include <queue>

namespace hycast {

class PortPool::Impl {
    typedef std::queue<in_port_t, std::list<in_port_t>> Queue;

    Queue queue;

public:
    Impl() = default;

    Impl(   const in_port_t min,
            const in_port_t max)
        : queue{}
    {
        for (in_port_t port = min; port <= max; ++port)
            queue.push(port);
    }

    int size() const
    {
        return queue.size();
    }

    in_port_t take()
    {
        if (queue.empty())
            throw std::range_error("Queue is empty");

        in_port_t port = queue.front();
        queue.pop();

        return port;
    }

    void add(const in_port_t port)
    {
        queue.push(port);
    }
};

/******************************************************************************/

PortPool::PortPool()
    : pImpl{}
{}

PortPool::PortPool(
        const in_port_t min,
        const in_port_t max)
    : pImpl{new Impl(min, max)}
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
