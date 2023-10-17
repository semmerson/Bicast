/**
 * This file implements a thread-safe delay-queue.
 *
 *       File: DelayQueue.cpp
 * Created on: 2017-05-23
 *     Author: Steven R. Emmerson
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
#include "DelayQueue.h"
#include "Shield.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <vector>

namespace bicast {

typedef std::chrono::steady_clock Clock;     ///< Type of clock used
typedef Clock::time_point         TimePoint; ///< Type of time point used

/**
 * Implementation of `DelayQueue`.
 * @tparam Value     Type of value being stored in the queue. Must support
 *                   copy assignment
 * @tparam Dur       Duration type (e.g., std::chrono::seconds)
 */
template<typename Value, typename Dur>
class DelayQueue<Value,Dur>::Impl final
{
    using Duration = Dur;
    using Mutex    = std::mutex;
    using Lock     = std::unique_lock<Mutex>;
    using Guard    = std::lock_guard<Mutex>;
    using Cond     = std::condition_variable;

    /**
     * An element in the queue.
     */
    class Element final {
        Value     value; /// The value.
        TimePoint when;  /// The reveal-time.

    public:
        /**
         * Constructs.
         *
         * @param[in] value  The value.
         * @param[in] when   The reveal-time
         */
        Element(
                const Value&     value,
                const TimePoint& when)
            : value{value}
            , when{when}
        {}

        /**
         * Returns the value.
         * @return            The value.
         * @exceptionsafety   Strong guarantee
         * @threadsafety      Safe
         * @cancellationpoint No
         */
        Value getValue() const noexcept
        {
            return value;
        }

        /**
         * Returns the reveal-time.
         * @return            The reveal-time.
         * @exceptionsafety   Strong guarantee
         * @threadsafety      Safe
         * @cancellationpoint No
         */
        const TimePoint& getTime() const noexcept
        {
            return when;
        }

        bool operator<(const Element& rhs) {
            // Later time => lower priority
            return when > rhs.when;
        }
    };

    using Queue = std::priority_queue<Element, std::vector<Element>>;

    mutable Mutex mutex;    ///< For concurrent access
    mutable Cond  cond;     ///< For signaling when the queue has been modified
    Queue         queue;    ///< The queue.
    bool          isClosed; ///< Whether or not the queue is closed

public:
    /**
     * Constructs.
     * @throws std::bad_alloc     If necessary memory can't be allocated.
     * @throws std::system_error  If a system error occurs.
     */
    explicit Impl()
        : mutex{}
        , cond{}
        , queue{}
        , isClosed{false}
    {}

    /**
     * Returns the number of elements in the queue.
     * @return The number of elements in the queue
     */
    size_t size() const {
        Guard guard{mutex};
        return queue.size();
    }

    /**
     * Adds a value to the queue.
     * @param[in] value              The value to be added
     * @param[in] when               The reveal-time
     * @throws    std::domain_error  `close()` was called
     * @exceptionsafety              Strong guarantee
     * @threadsafety                 Safe
     */
    void push(
            const Value&     value,
            const TimePoint& when)
    {
        Guard guard{mutex};

        if (isClosed)
            throw DOMAIN_ERROR("DelayQueue is closed");

        queue.push(Element(value, when));
        cond.notify_all();
    }

    /**
     * Returns the value whose reveal-time is the earliest and not later than
     * the current time and removes it from the queue. Blocks until such a value
     * is available or `close()` is called.
     *
     * @return                    The value with the earliest reveal-time that's
     *                            not later than the current time.
     * @throws std::domain_error  `close()` was called
     * @exceptionsafety           Strong guarantee
     * @threadsafety              Safe
     * @cancellationpoint
     * @see `close()`
     */
    Value pop()
    {
        static auto pred = [&]{return isClosed || queue.top().getTime() >= Clock::now();};
        Lock        lock{mutex};

        if (queue.empty())
            cond.wait(lock, [&]{return isClosed || !queue.empty();});
        if (cond.wait_until(lock, queue.top().getTime(), pred) {
            break;
        }

        if (isClosed)
            throw DOMAIN_ERROR("DelayQueue is closed");

        Shield shield{};
        Value    value = queue.top().getValue();
        queue.pop();

        return value;
    }

    /**
     * @cancellationpoint No
     */
    bool ready() const //noexcept
    {
        Guard guard{mutex};
        return !queue.empty() && queue.top().getTime() <= Clock::now();
    }

    /**
     * @cancellationpoint No
     */
    bool empty() const //noexcept
    {
        Guard guard{mutex};
        return queue.empty();
    }

    /**
     * @cancellationpoint No
     */
    void clear() //noexcept
    {
        Guard guard{mutex};
        while (!queue.empty())
            queue.pop();
        cond.notify_all();
    }

    /**
     * Closes the queue. Upon return, no more elements will be added.
     */
    void close() {
        Guard guard{mutex};

        while (!queue.empty())
            queue.pop();

        isClosed = true;
        cond.notify_all();
    }
};

template<typename Value, typename Dur>
DelayQueue<Value, Dur>::DelayQueue()
    : pImpl{new Impl{}}
{}

template<typename Value, typename Dur>
size_t DelayQueue<Value, Dur>::size() const {
    return pImpl->size();
}

template<typename Value, typename Dur>
void DelayQueue<Value, Dur>::push(
        const Value& value,
        const int    delay) const {
    auto now = Clock::now();
    auto delta = Dur{delay};
    auto when = now + delta;
    pImpl->push(value, when);
}

template<typename Value, typename Dur>
Value DelayQueue<Value, Dur>::pop() const {
    return pImpl->pop();
}

template<typename Value, typename Dur>
bool DelayQueue<Value, Dur>::ready() const {
    return pImpl->ready();
}

template<typename Value, typename Dur>
bool DelayQueue<Value, Dur>::empty() const {
    return pImpl->empty();
}

template<typename Value, typename Dur>
void DelayQueue<Value, Dur>::clear() {
    return pImpl->clear();
}

template<typename Value, typename Dur>
void DelayQueue<Value, Dur>::close() {
    pImpl->close();
}

} // namespace
