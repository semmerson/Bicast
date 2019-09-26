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

#include "config.h"

#include "NoticeQueue.h"

#include <condition_variable>
#include <list>
#include <mutex>
#include <queue>

namespace hycast {

class NoticeQueue::Impl {
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;

    Mutex                                   mutex;
    Cond                                    cond;
    std::queue<ChunkId, std::list<ChunkId>> queue;

public:
    bool push(const ChunkId& chunkId)
    {
        Guard guard(mutex);

        queue.push(chunkId);
        cond.notify_all();

        return true;
    }

    ChunkId pop()
    {
        Lock lock(mutex);

        while (queue.empty())
            cond.wait(lock);

        ChunkId chunkId = queue.front();
        queue.pop();
        cond.notify_all();

        return chunkId;
    }
};

/******************************************************************************/

NoticeQueue::NoticeQueue()
    : pImpl{new Impl()}
{}

bool NoticeQueue::push(const ChunkId& chunkId)
{
    return pImpl->push(chunkId);
}

ChunkId NoticeQueue::pop()
{
    return pImpl->pop();
}

} // namespace
