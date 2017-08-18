/**
 * This file implements a source of potential remote peers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSsource.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "DelayQueue.h"
#include "error.h"
#include "PeerSource.h"

#include <set>

namespace hycast {

class PeerSource::Impl
{
    DelayQueue<InetSockAddr, Duration>   queue;

public:
    /**
     * Default constructs.
     */
    Impl()
        : queue{}
    {}

    void push(
            const InetSockAddr& peerAddr,
            const Duration&     delay)
    {
        return queue.push(peerAddr, delay);
    }

    InetSockAddr pop()
    {
        return queue.pop();
    }

    bool empty() const noexcept
            {
        return queue.empty();
    }
};

PeerSource::PeerSource()
    : pImpl{new Impl()}
{}

PeerSource::~PeerSource()
{}

void PeerSource::push(
        const InetSockAddr& peerAddr,
        const Duration&     delay)
{
    pImpl->push(peerAddr, delay);
}

InetSockAddr PeerSource::pop()
{
    return pImpl->pop();
}

bool PeerSource::empty() const noexcept
{
    return pImpl->empty();
}

} // namespace
