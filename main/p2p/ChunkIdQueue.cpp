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

    mutable Mutex       mutex;
    mutable Cond        cond;
    std::queue<ChunkId> queue;
    bool                isClosed;

public:
    Impl()
        : mutex{}
        , cond{}
        , queue{}
        , isClosed{false}
    {}

    size_t size() const noexcept
    {
        Guard guard(mutex);
        return queue.size();
    }

    void push(const ChunkId chunkId)
    {
        Guard guard(mutex);
        if (!isClosed) {
            queue.push(chunkId);
            cond.notify_one();
        }
    }

    ChunkId pop()
    {
        try {
            Lock lock{mutex};
            while (!isClosed && queue.empty())
                cond.wait(lock);
            if (isClosed)
                return ChunkId();
            ChunkId notice{queue.front()};
            queue.pop();
            return notice;
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't pop notice-queue"));
        }
    }

    void close() noexcept
    {
        Guard guard{mutex};
        isClosed = true;
        while (!queue.empty())
            queue.pop();
        cond.notify_all();
    }

    bool closed() const noexcept
    {
        Guard guard{mutex};
        return isClosed;
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

void ChunkIdQueue::close() const noexcept
{
    pImpl->close();
}

bool ChunkIdQueue::closed() const noexcept
{
    return pImpl->closed();
}

} // namespace
