/**
 * A thread-safe queue of notices to be sent.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: NoticeQueue.cpp
 *  Created on: Jun 18, 2019
 *      Author: Steven R. Emmerson
 */

#include <IdQueue.h>
#include "config.h"
#include "error.h"
#include <condition_variable>
#include <mutex>
#include <queue>

namespace hycast {

void ThingId::notify(PeerProto& peerProto) const
{
    if (isProd) {
        LOG_DEBUG("Notifying about product %lu",
                static_cast<unsigned long>(id.prodIndex));
        peerProto.notify(id.prodIndex);
    }
    else {
        LOG_DEBUG("Notifying about segment %s",
                id.segId.to_string().c_str());
        peerProto.notify(id.segId);
    }
}

void ThingId::request(PeerProto& peerProto) const
{
    if (isProd) {
        LOG_DEBUG("Requesting information about product %lu",
                static_cast<unsigned long>(id.prodIndex));
        peerProto.request(id.prodIndex);
    }
    else {
        LOG_DEBUG("Requesting segment %s",
                id.segId.to_string().c_str());
        peerProto.request(id.segId);
    }
}

/******************************************************************************/

class IdQueue::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;

    mutable Mutex      mutex;
    mutable Cond       cond;
    std::queue<ThingId> queue;

public:
    Impl()
        : mutex{}
        , cond{}
        , queue{}
    {}

    size_t size() const noexcept
    {
        Guard guard(mutex);
        return queue.size();
    }

    void push(const ProdIndex index)
    {
        Guard guard(mutex);
        queue.push(ThingId{index});
        cond.notify_one();
    }

    void push(const SegId& segId)
    {
        Guard guard(mutex);
        queue.push(ThingId{segId});
        cond.notify_one();
    }

    ThingId pop()
    {
        Lock lock{mutex};
        while (queue.empty())
            cond.wait(lock);
        ThingId notice{queue.front()};
        queue.pop();
        return notice;
    }
};

IdQueue::IdQueue()
    : pImpl{new Impl()}
{}

size_t IdQueue::size() const noexcept
{
    return pImpl->size();
}

void IdQueue::push(const ProdIndex index) const
{
    pImpl->push(index);
}

void IdQueue::push(const SegId& segId) const
{
    pImpl->push(segId);
}

ThingId IdQueue::pop() const
{
    return pImpl->pop();
}

} // namespace
