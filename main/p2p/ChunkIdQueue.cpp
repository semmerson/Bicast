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

#include "ChunkIdQueue.h"
#include "config.h"
#include "error.h"

#include <condition_variable>
#include <mutex>
#include <queue>

namespace hycast {

class ChunkIdQueue::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;

    mutable Mutex      mutex;
    mutable Cond       cond;
    std::queue<ChunkId> queue;

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

    void push(const ChunkId chunkId)
    {
        Guard guard(mutex);
        queue.push(chunkId);
        cond.notify_one();
    }

    ChunkId pop()
    {
        Lock lock{mutex};
        while (queue.empty())
            cond.wait(lock);
        ChunkId notice{queue.front()};
        queue.pop();
        return notice;
    }
};

ChunkIdQueue::ChunkIdQueue()
    : pImpl{new Impl()}
{}

size_t ChunkIdQueue::size() const noexcept
{
    return pImpl->size();
}

void ChunkIdQueue::push(const ChunkId chunkId) const
{
    pImpl->push(chunkId);
}

ChunkId ChunkIdQueue::pop() const
{
    return pImpl->pop();
}

} // namespace
