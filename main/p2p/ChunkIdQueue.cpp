/**
 * A thread-safe queue of notices to be sent.
 *
 *        File: NoticeQueue.cpp
 *  Created on: Jun 18, 2019
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
    typedef std::deque<ChunkId>     Queue;

    mutable Mutex mutex;
    mutable Cond  cond;
    Queue         queue;
    bool          isClosed;

public:
    typedef Queue::iterator Iterator;

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
            queue.push_back(chunkId);
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
            ChunkId chunkId{queue.front()};
            queue.pop_front();
            return chunkId;
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't pop notice-queue"));
        }
    }

    void close() noexcept
    {
        Guard guard{mutex};
        isClosed = true;
        cond.notify_all();
    }

    bool closed() const noexcept
    {
        Guard guard{mutex};
        return isClosed;
    }

    Iterator begin()
    {
        return queue.begin();
    }

    Iterator end()
    {
        return queue.end();
    }
};

class ChunkIdQueue::Iterator::Impl
    : public std::iterator<std::input_iterator_tag, ChunkId>
{
    ChunkIdQueue::Impl::Iterator iter;

public:
    Impl(const ChunkIdQueue::Impl::Iterator& iter)
        : iter(iter)
    {}

    Impl(const ChunkIdQueue::Impl::Iterator&& iter)
        : iter(iter)
    {}

    Impl(const Impl& that)
        : iter(that.iter)
    {}

    Impl& operator=(const Impl& rhs)
    {
        iter = rhs.iter;
        return *this;
    }

    bool operator==(const Impl& rhs)
    {
        return iter == rhs.iter;
    }

    bool operator!=(const Impl& rhs)
    {
        return iter != rhs.iter;
    }

    ChunkId operator*()
    {
        return *iter;
    }

    Impl& operator++()
    {
        ++iter;
        return *this;
    }

    Impl operator++(int)
    {
        Impl tmp(*this);
        ++iter;
        return tmp;
    }
};

ChunkIdQueue::Iterator::Iterator(Impl* impl)
    : pImpl(impl)
{}

ChunkIdQueue::Iterator::Iterator(const Iterator& that)
    : pImpl(new Impl(*pImpl))
{}

ChunkIdQueue::Iterator& ChunkIdQueue::Iterator::operator=(const Iterator& rhs)
{
    pImpl.reset(new Impl(*rhs.pImpl));
    return *this;
}

bool ChunkIdQueue::Iterator::operator==(const Iterator& rhs)
{
    return *pImpl == *rhs.pImpl;
}

bool ChunkIdQueue::Iterator::operator!=(const Iterator& rhs)
{
    return *pImpl != *rhs.pImpl;
}

ChunkId ChunkIdQueue::Iterator::operator*()
{
    return **pImpl;
}

ChunkIdQueue::Iterator& ChunkIdQueue::Iterator::operator++()
{
    ++*pImpl;
    return *this;
}

ChunkIdQueue::Iterator ChunkIdQueue::Iterator::operator++(int)
{
    Iterator tmp(*this);
    ++*pImpl;
    return tmp;
}

/******************************************************************************/

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

ChunkIdQueue::Iterator ChunkIdQueue::begin()
{
    return Iterator(new Iterator::Impl(pImpl->begin()));
}

ChunkIdQueue::Iterator ChunkIdQueue::end()
{
    return Iterator(new Iterator::Impl(pImpl->end()));
}

} // namespace
