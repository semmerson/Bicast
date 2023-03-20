/**
 * This file declares a thread-safe delay-queue. Each element has a time-point
 * (a reveal-time) when it becomes available.
 *
 *        File: DelayQueue.h
 *  Created on: May 23, 2017
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

#ifndef MISC_DELAYQUEUE_H
#define MISC_DELAYQUEUE_H

#include "InetAddr.h"

#include <chrono>
#include <iterator>
#include <memory>

namespace hycast {

/**
 * @tparam VALUE     Type of value being stored in the queue. Must support
 *                   copy assignment and move assignment.
 * @tparam DUR       Duration unit for integer duration arguments to `push()`
 */
template<typename VALUE, typename DUR = std::chrono::seconds>
class DelayQueue final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    using  Duration = DUR; ///< Duration type

    /// Iterator for a delay-queue
    class Iterator : public std::iterator<std::input_iterator_tag, VALUE> {
    public:
        /**
         *  Constructs an iterator to the beginning of the queue.
         */
        Iterator(VALUE* x);
        /**
         *  Constructs a constant iterator to the beginning of the queue.
         */
        Iterator(const Iterator& mit);
        Iterator& operator++(); ///< Pre-increments to point to the next element
        Iterator operator++(int); ///< Post-increments to point to the next element
        bool operator==(const Iterator& rhs) const; ///< Equality operator
        bool operator!=(const Iterator& rhs) const; ///< Inequality operator
        VALUE& operator*(); ///< Dereferences the pointed-to element
    };

    /**
     * Default constructs.
     * @throws std::bad_alloc     If necessary memory can't be allocated.
     * @throws std::system_error  If a system error occurs.
     */
    DelayQueue();

    /**
     * Returns the number of elements.
     *
     * @return  Number of elements
     */
    size_t size() const;

    /**
     * Adds a value to the queue.
     * @param[in] value              The value to be added
     * @param[in] delay              The delay for the element before it becomes
     *                               available in units of the duration template
     *                               parameter
     * @throws    std::domain_error  `close()` has been called
     * @exceptionsafety              Strong guarantee
     * @threadsafety                 Safe
     */
    void push(
            const VALUE& value,
            const int    delay = 0) const;

    /**
     * Returns the value whose reveal-time is the earliest but not earlier than
     * the current time and removes it from the queue. Blocks until such a value
     * is available.
     * @return                       The value with the earliest reveal-time
     *                               that's not later than the current time.
     * @throws std::domain_error     `close()` has been called
     * @exceptionsafety              Strong guarantee
     * @threadsafety                 Safe
     * @cancellationpoint
     */
    VALUE pop() const;

    /**
     * Indicates if `pop()` will immediately return.
     *
     * @retval true       Yes
     * @retval false      No
     * @cancellationpoint No
     */
    bool ready() const;

    /**
     * Indicates if the queue is empty.
     * @return true       The queue is empty
     * @return false      The queue is not empty
     * @exceptionsafety   Nothrow
     * @threadsafety      Safe
     * @cancellationpoint No
     */
    bool empty() const;

    /**
     * Clears the queue of all elements.
     * @exceptionsafety   Nothrow
     * @threadsafety      Safe
     * @cancellationpoint No
     */
    void clear();

    /**
     * Closes the queue. Causes `pop()` and `push()` to throw an exception.
     * Idempotent.
     */
    void close();

    /**
     * Returns an iterator to the beginning of the queue.
     * @return An iterator to the beginning of the queue
     */
    Iterator begin();

    /**
     * Returns a constant iterator to the beginning of the queue.
     * @return A constant iterator to the beginning of the queue
     */
    Iterator begin() const;

    /**
     * Returns an iterator to just beyond the end of the queue.
     * @return An iterator to just beyond the end of the queue
     */
    Iterator end();

    /**
     * Returns a constant iterator to just beyond the end of the queue.
     * @return A constant iterator to just beyond the end of the queue
     */
    Iterator end() const;
};

} // namespace

#include "DelayQueue.cpp" // For automatic template instantiation

#endif /* MISC_DELAYQUEUE_H */
