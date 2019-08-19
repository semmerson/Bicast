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
#include <deque>
#include <mutex>
#include <queue>

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
         * @return  The value.
         * @exceptionsafety Strong guarantee
         * @threadsafety    Safe
         */
        Value getValue() const noexcept
        {
            return value;
        }

        /**
         * Returns the reveal-time.
         * @return  The reveal-time.
         * @exceptionsafety Strong guarantee
         * @threadsafety    Safe
         */
        const TimePoint& getTime() const noexcept
        {
            return when;
        }
    };

    /**
     * Class for comparing elements.
     */
    struct Compare final
    {
        /**
         * Compares two elements.
         * @param[in] e1   First element
         * @param[in] e2   Second element
         * @return `true`  First element is less than second element
         * @return `false` First element is not less than second element
         */
        bool operator()(const Element& e1, const Element& e2) const noexcept
        {
            // Later time => lower priority
            return e1.getTime() > e2.getTime();
        }
    };

    /// The mutex for concurrent access to the queue.
    std::mutex mutable                                         mutex;
    /// The condition variable for signaling when the queue has been modified
    std::condition_variable                                    cond;
    /// The queue.
    std::priority_queue<Element, std::deque<Element>, Compare> queue;

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
    {}

    /**
     * Adds a value to the queue.
     * @param[in] value  The value to be added
     * @param[in] when   The reveal-time
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void push(
            const Value&     value,
            const TimePoint& when)
    {
        Guard guard{mutex};
        queue.push(Element(value, when));
        cond.notify_one();
    }

    /**
     * Returns the value whose reveal-time is the earliest and not later than
     * the current time and removes it from the queue. Blocks until such a value
     * is available.
     *
     * @return          The value with the earliest reveal-time that's not later
     *                  than the current time.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Safe
     */
    Value pop()
    {
        Lock lock{mutex};

        for (;;) {
            if (queue.empty()) {
                Canceler canceler{};
                cond.wait(lock);
            }
            else {
                auto revealTime = queue.top().getTime();

                if (revealTime <= Clock::now()) {
                    break;
                }
                else {
                    Canceler canceler{};
                    cond.wait_until(lock, revealTime);
                }
            }
        }

        auto value = queue.top().getValue();
        queue.pop();

        return value;
    }

    bool ready() const noexcept
    {
        Guard guard{mutex};
        return !queue.empty() && queue.top().getTime() <= Clock::now();
    }

    bool empty() const noexcept
    {
        Guard guard{mutex};
        return queue.empty();
    }

    void clear() noexcept
    {
        Guard guard{mutex};
        while (!queue.empty())
            queue.pop();
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
bool DelayQueue<Value, Dur>::ready() const noexcept
{
    return pImpl->ready();
}

template<typename Value, typename Dur>
bool DelayQueue<Value, Dur>::empty() const noexcept
{
    return pImpl->empty();
}

template<typename Value, typename Dur>
void DelayQueue<Value, Dur>::clear() noexcept
{
    return pImpl->clear();
}

} // namespace
