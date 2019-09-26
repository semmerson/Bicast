/**
 * This file implements a thread-safe delay-queue.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *       File: DelayQueue.cpp
 * Created on: 2017-05-23
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "DelayQueue.h"
#include "Thread.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <vector>

namespace hycast {

typedef std::chrono::steady_clock Clock;
typedef Clock::time_point         TimePoint;

/**
 * Implementation of `DelayQueue`.
 * @tparam Value     Type of value being stored in the queue. Must support
 *                   copy assignment
 * @tparam Dur       Duration type (e.g., std::chrono::seconds)
 */
template<typename Value, typename Dur>
class DelayQueue<Value,Dur>::Impl final
{
    typedef Dur                       Duration;
    typedef std::mutex                Mutex;
    typedef std::unique_lock<Mutex>   Lock;
    typedef std::lock_guard<Mutex>    Guard;

    /**
     * An element in the queue.
     */
    class Element final {
        Value     value; /// The value.
        TimePoint when;  /// The reveal-time.

    public:
        /**
         * Constructs from a value and a delay.
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

    struct Compare final {
        /**
         * @cancellationpoint No
         */
        bool operator()(const Element& lhs, const Element& rhs) const noexcept {
            return lhs.getTime() > rhs.getTime();
        }
    };

    /// The mutex for concurrent access to the queue.
    std::mutex mutable                                          mutex;
    /// The condition variable for signaling when the queue has been modified
    std::condition_variable                                     cond;
    /// The queue.
    std::priority_queue<Element, std::vector<Element>, Compare> queue;
    /// Whether or not the queue is closed
    bool                                                        isClosed;

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
     * is available.
     *
     * @return                    The value with the earliest reveal-time that's
     *                            not later than the current time.
     * @throws std::domain_error  `close()` was called
     * @exceptionsafety           Strong guarantee
     * @threadsafety              Safe
     * @cancellationpoint
     */
    Value pop()
    {
        Value value;
        Lock  lock{mutex};

        for (;;) {
            LOG_DEBUG("isClosed: %d", isClosed);
            if (isClosed) {
                LOG_DEBUG("Throwing DOMAIN_ERROR");
                throw DOMAIN_ERROR("DelayQueue is closed");
            }

            if (queue.empty()) {
                LOG_DEBUG("Waiting");
                //Canceler canceler{true};
                try {
                    cond.wait(lock);
                }
                catch (const std::exception& ex) {
                    LOG_DEBUG("Caught std::exception");
                    throw;
                }
                catch (...) {
                    LOG_DEBUG("Caught ... exception");
                    throw;
                }
            }
            else {
                auto revealTime = queue.top().getTime();

                if (revealTime > Clock::now()) {
                    LOG_DEBUG("Waiting until");
                    cond.wait_until(lock, revealTime);
                }
                else {
                    LOG_DEBUG("Popping value");
                    Canceler canceler{false};
                    value = queue.top().getValue();
                    queue.pop();
                    break;
                }
            }
        }

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
void DelayQueue<Value, Dur>::push(
        const Value& value,
        const int    delay) const
{
    auto now = Clock::now();
    auto delta = Dur{delay};
    auto when = now + delta;
    pImpl->push(value, when);
}

template<typename Value, typename Dur>
Value DelayQueue<Value, Dur>::pop() const
{
    return pImpl->pop();
}

template<typename Value, typename Dur>
bool DelayQueue<Value, Dur>::ready() const //noexcept
{
    return pImpl->ready();
}

template<typename Value, typename Dur>
bool DelayQueue<Value, Dur>::empty() const //noexcept
{
    return pImpl->empty();
}

template<typename Value, typename Dur>
void DelayQueue<Value, Dur>::clear() //noexcept
{
    return pImpl->clear();
}

template<typename Value, typename Dur>
void DelayQueue<Value, Dur>::close()
{
    pImpl->close();
}

} // namespace
