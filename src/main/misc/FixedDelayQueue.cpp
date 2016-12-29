/**
 * This file implements a thread-safe, fixed-duration, delay-queue.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: FixedDelayQueue.cpp
 * @author: Steven R. Emmerson
 */

#include <FixedDelayQueue.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

namespace hycast {

/**
 * Implementation of `FixedDelayQueue`.
 * @tparam Value     Type of value being stored in the queue. Must support
 *                   copy assignment
 * @tparam Rep       Representation of delay counts (e.g.,
 *                   `std::chrono::seconds::rep`)
 * @tparam Period    Unit of delay (e.g., `std::chrono::seconds::period`)
 */
template<typename Value, typename Rep, typename Period>
class FixedDelayQueueImpl final
{
    typedef std::chrono::duration<Rep, Period> Duration;
    typedef std::chrono::steady_clock          Clock;
    typedef Clock::time_point                  TimePoint;

    /**
     * An element in the queue.
     */
    class Element final {
        /// The value.
        Value     value;
        /// The reveal-time.
        TimePoint when;

    public:
        /**
         * Constructs from a value and a delay.
         * @param[in] value  The value.
         * @param[in] delay  The delay for the element until the reveal-time.
         *                   May be negative.
         */
        Element(
                Value          value,
                const Duration delay)
            : value{value}
            , when{Clock::now() + delay}
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

    /// The mutex for protecting the queue.
    std::mutex              mutex;
    /// The condition variable for signaling when the queue has been modified
    std::condition_variable cond;
    /// The queue.
    std::queue<Element>     queue;
    /// Residence-time (i.e., delay-time) for an element in the queue
    Duration                delay;

public:
    /**
     * Constructs from a delay.
     * @param[in] delay  The delay for each element in units of the template
     *                   parameter
     * @throws std::bad_alloc     If necessary memory can't be allocated.
     * @throws std::system_error  If a system error occurs.
     */
    explicit FixedDelayQueueImpl(const Duration delay)
        : mutex{}
        , cond{}
        , queue{}
        , delay{delay}
    {}

    /**
     * Adds a value to the queue.
     * @param[in] value The value to be added
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void push(Value value)
    {
        std::unique_lock<std::mutex>(mutex);
        queue.push(Element(value, delay));
        cond.notify_one();
    }

    /**
     * Returns the value whose reveal-time is the earliest and not later than
     * the current time and removes it from the queue. Blocks until such a value
     * is available.
     * @return  The value with the earliest reveal-time that's not later than
     *          the current time.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Safe
     */
    Value pop()
    {
        std::unique_lock<std::mutex> lock(mutex);
        while (queue.size() == 0)
            cond.wait(lock);
        for (const TimePoint time = queue.front().getTime(); time > Clock::now(); )
            cond.wait_until(lock, time);
        Value value = queue.front().getValue();
        queue.pop();
        cond.notify_one();
        return value;
    }

    /**
     * Returns the number of values in the queue.
     * @return  The number of values in the queue.
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t size() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }
};

template<typename Value, typename Rep, typename Period>
FixedDelayQueue<Value, Rep, Period>::FixedDelayQueue(const Duration delay)
    : pImpl{new FixedDelayQueueImpl<Value, Rep, Period>{delay}}
{}

template<typename Value, typename Rep, typename Period>
void FixedDelayQueue<Value, Rep, Period>::push(Value value)
{
    pImpl->push(value);
}

template<typename Value, typename Rep, typename Period>
Value FixedDelayQueue<Value, Rep, Period>::pop()
{
    return pImpl->pop();
}

template<typename Value, typename Rep, typename Period>
size_t FixedDelayQueue<Value, Rep, Period>::size() const noexcept
{
    return pImpl->size();
}

} // namespace
