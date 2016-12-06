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

#include "FixedDelayQueue.h"

namespace hycast {

template<typename Value, typename Rep, typename Period>
FixedDelayQueue<Value, Rep, Period>::Element::Element(
        Value          value,
        const Duration delay)
    : value{value}
    , when{Clock::now() + delay}
{}

template<typename Value, typename Rep, typename Period>
FixedDelayQueue<Value, Rep, Period>::FixedDelayQueue(const Duration delay)
    : mutex{}
    , cond{}
    , queue{}
    , delay{delay}
{}

template<typename Value, typename Rep, typename Period>
void FixedDelayQueue<Value, Rep, Period>::push(Value value)
{
    std::unique_lock<std::mutex>(mutex);
    queue.push(Element(value, delay));
    cond.notify_one();
}

template<typename Value, typename Rep, typename Period>
Value FixedDelayQueue<Value, Rep, Period>::pop()
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

template<typename Value, typename Rep, typename Period>
size_t FixedDelayQueue<Value, Rep, Period>::size() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex);
    return queue.size();
}

} // namespace
