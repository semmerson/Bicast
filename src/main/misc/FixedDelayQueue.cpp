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
#include <FixedDelayQueueImpl.h>

namespace hycast {

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
