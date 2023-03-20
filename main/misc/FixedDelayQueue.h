/**
 * This file declares a thread-safe, fixed-duration, delay-queue.
 *
 *   @file: FixedDelayQueue.h
 * @author: Steven R. Emmerson
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

#ifndef MISC_FIXEDDELAYQUEUE_H
#define MISC_FIXEDDELAYQUEUE_H

#include "Thread.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>

namespace hycast {

/**
 * @tparam Value     Type of value being stored in the queue. Must support
 *                   copy assignment and move assignment.
 * @tparam Dur       Duration type (e.g., std::chrono::seconds)
 */
template<typename Value, typename Dur>
class FixedDelayQueue final
{
public:
    typedef Dur                       Duration; ///< The duration type

private:
    typedef std::chrono::steady_clock Clock;
    typedef Clock::time_point         TimePoint;
    /**
     * Implementation of `FixedDelayQueue`.
     */
    class Impl final
    {
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

        /// The mutex for concurrent access to the queue.
        std::mutex mutable      mutex;
        /// The condition variable for signaling when the queue has been modified
        std::condition_variable cond;
        /// The queue.
        std::queue<Element>     queue;
        /// Minimum residence-time (i.e., delay-time) for an element in the queue
        Duration                delay;

    public:
        /**
         * Constructs from a delay.
         * @param[in] delay  The delay for each element in units of the template
         *                   parameter
         * @throws std::bad_alloc     If necessary memory can't be allocated.
         * @throws std::system_error  If a system error occurs.
         */
        explicit Impl(const Duration delay)
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
            while (queue.size() == 0) {
                Canceler canceler{};
                cond.wait(lock);
            }
            for (const TimePoint time = queue.front().getTime();
                    time > Clock::now(); ) {
                Canceler canceler{};
                cond.wait_until(lock, time);
            }
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
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs from a delay.
     * @param[in] delay  The delay for each element in units of the template
     *                   parameter
     * @throws std::bad_alloc     If necessary memory can't be allocated.
     * @throws std::system_error  If a system error occurs.
     */
    explicit FixedDelayQueue(Dur delay)
        : pImpl{new Impl{delay}}
    {}

    /**
     * Adds a value to the queue.
     * @param[in] value The value to be added
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void push(Value value)
    {
        pImpl->push(value);
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
        return pImpl->pop();
    }

    /**
     * Returns the number of values in the queue.
     * @return  The number of values in the queue.
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t size() const noexcept
    {
        return pImpl->size();
    }
};

} // namespace

#endif /* MISC_FIXEDDELAYQUEUE_H */
