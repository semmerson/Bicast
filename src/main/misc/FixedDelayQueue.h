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
#include <memory>

namespace hycast {

template<typename Value, typename Rep, typename Period>
class FixedDelayQueueImpl; // Forward declaration

/**
 * @tparam Value     Type of value being stored in the queue. Must support
 *                   copy assignment and move assignment.
 * @tparam Duration  Unit of delay (e.g., `std::chrono::seconds`)
 */
template<typename Value, typename Rep, typename Period>
class FixedDelayQueue final {
    typedef std::chrono::duration<Rep, Period> Duration;

    /// `pImpl` idiom
    std::shared_ptr<FixedDelayQueueImpl<Value, Rep, Period>> pImpl;

public:
    /**
     * Constructs from a delay.
     * @param[in] delay  The delay for each element in units of the template
     *                   parameter
     * @throws std::bad_alloc     If necessary memory can't be allocated.
     * @throws std::system_error  If a system error occurs.
     */
    explicit FixedDelayQueue(Duration delay);
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
     * @return  The number of values in the queue.
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t size() const noexcept;
};

} // namespace

#endif /* MISC_FIXEDDELAYQUEUE_H */
