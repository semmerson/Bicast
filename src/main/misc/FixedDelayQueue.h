/**
 * This file declares a thread-safe, fixed-duration, delay-queue.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: FixedDelayQueue.h
 * @author: Steven R. Emmerson
 */

#ifndef MISC_FIXEDDELAYQUEUE_H
#define MISC_FIXEDDELAYQUEUE_H

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

namespace hycast {

/**
 * @tparam Value     Type of value being stored in the queue. Must support
 *                   copy assignment.
 * @tparam Duration  Unit of delay (e.g., `std::chrono::duration::seconds`)
 */
template<typename Value, typename Duration>
class FixedDelayQueue final {
    typedef std::chrono::steady_clock Clock;
    typedef Clock::time_point         TimePoint;

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
        Element(Value value, const Duration delay);
        /**
         * Returns the value.
         * @return  The value.
         * @exceptionsafety Strong guarantee
         * @threadsafety    Safe
         */
        Value getValue() const noexcept {return value;}
        /**
         * Returns the reveal-time.
         * @return  The reveal-time.
         * @exceptionsafety Strong guarantee
         * @threadsafety    Safe
         */
        const TimePoint& getTime() const noexcept {return when;}
    };

    /// The mutex for protecting the queue.
    std::mutex              mutex;
    /// The condition variable for signaling when the queue has been modified
    std::condition_variable cond;
    /// The queue.
    std::queue<Element>     queue;
    /// Residence-time for an element in the queue
    Duration                delay;

public:
    /**
     * Constructs from a delay.
     * @param[in] delay  The delay for each element in units of the template
     *                   parameter
     * @throws std::bad_alloc     If necessary memory can't be allocated.
     * @throws std::system_error  If a system error occurs.
     */
    FixedDelayQueue(Duration delay);
    /**
     * Adds a value to the queue.
     * @param[in] value The value to be added
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void push(Value value);
    /**
     * Returns the value whose reveal-time is the earliest and not later than
     * the current time and removes it from the queue. Blocks until such a value
     * is available.
     * @return  The value with the earliest reveal-time that's not later than
     *          the current time.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Safe
     */
    Value pop();
    /**
     * Returns the number of values in the queue.
     *
     * @return  The number of values in the queue.
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t size() const noexcept;
};

} // namespace

#endif /* MISC_FIXEDDELAYQUEUE_H */
