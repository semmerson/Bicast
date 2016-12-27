/**
 * This file implements the implementation of a future of an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: FutureImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "FutureImpl.h"

namespace hycast {

template<class R>
BasicFutureImpl<R>::BasicFutureImpl()
    : func{}
    , mutex{}
    , cond{}
    , exception{}
    , threadId{}
    , state{}
{}

template<class R>
BasicFutureImpl<R>::BasicFutureImpl(std::function<R()> func)
    : func{func}
    , mutex{}
    , cond{}
    , exception{}
    , threadId{}
    , state{}
{}

template<class R>
BasicFutureImpl<R>::~BasicFutureImpl()
{}

template<class R>
void BasicFutureImpl<R>::taskCancelled() {
    std::lock_guard<decltype(mutex)> lock(mutex);
    state[State::CANCELLED] = true;
    state[State::COMPLETED] = true;
    cond.notify_one();
}

template<class R>
void BasicFutureImpl<R>::taskCancelled(void* arg) {
    auto future = reinterpret_cast<BasicFutureImpl<R>*>(arg);
    future->taskCancelled();
}

template<class R>
void BasicFutureImpl<R>::execute() {
    std::unique_lock<decltype(mutex)> lock(mutex);
    if (state[State::THREAD_ID_SET])
        throw std::logic_error("Task already executed");
    threadId = pthread_self();
    state[State::THREAD_ID_SET] = true;
    cond.notify_one();
    if (!state[State::CANCELLED]) {
        pthread_cleanup_push(taskCancelled, this);
        lock.unlock(); // Unlocked to enable cancellation
        try {
            setResult();
        }
        catch (const std::exception& e) {
            exception = std::current_exception();
        }
        lock.lock();
        pthread_cleanup_pop(0);
    }
    state[State::COMPLETED] = true;
    cond.notify_one();
}

template<class R>
pthread_t BasicFutureImpl<R>::getThreadId() const
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    while (!state[State::THREAD_ID_SET])
        cond.wait(lock);
    return threadId;
}

template<class R>
void BasicFutureImpl<R>::cancel() {
    std::lock_guard<decltype(mutex)> lock(mutex);
    if (!state[State::THREAD_ID_SET]) {
        // Task hasn't started
        state[State::CANCELLED] = true;
        state[State::COMPLETED] = true;
        cond.notify_one();
    }
    else if (!state[State::COMPLETED]) {
        int status = pthread_cancel(threadId);
        if (status)
            throw std::system_error(status, std::system_category(),
                    "Couldn't cancel asynchronous task's thread");
    }
}

template<class R>
void BasicFutureImpl<R>::wait() {
    std::unique_lock<decltype(mutex)> lock(mutex);
    if (!state[State::JOINED]) {
        while (!state[State::THREAD_ID_SET])
            cond.wait(lock);
        lock.unlock(); // So task's thread can acquire mutex
        int status = pthread_join(threadId, nullptr);
        if (status)
            throw std::system_error(errno, std::system_category(),
                    "Couldn't join thread");
        lock.lock();
        state[State::JOINED] = true;
        cond.notify_one();
    }
}

template<class R>
bool BasicFutureImpl<R>::wasCancelled() {
    wait();
    return state[State::CANCELLED];
}

template<class R>
FutureImpl<R>::FutureImpl()
    : result{}
{}

template<class R>
FutureImpl<R>::FutureImpl(std::function<R()> func)
    : BasicFutureImpl<R>::BasicFutureImpl(func)
    , result{}
{}

template<class R>
void FutureImpl<R>::setResult() {
    result = func();
}

template<class R>
R FutureImpl<R>::getResult() {
    wait();
    if (exception)
        std::rethrow_exception(exception);
    if (state[BasicFutureImpl<R>::State::CANCELLED])
        throw std::logic_error("Asynchronous task was cancelled");
    return result;
}

FutureImpl<void>::FutureImpl()
    : BasicFutureImpl<void>::BasicFutureImpl()
{}

FutureImpl<void>::FutureImpl(std::function<void()> func)
    : BasicFutureImpl<void>::BasicFutureImpl(func)
{}

void FutureImpl<void>::setResult() {
    func();
}

void FutureImpl<void>::getResult() {
    wait();
    if (exception)
        std::rethrow_exception(exception);
    if (state[State::CANCELLED])
        throw std::logic_error("Asynchronous task was cancelled");
}

template class BasicFutureImpl<int>;
template class BasicFutureImpl<void>;

template class FutureImpl<int>;
template class FutureImpl<void>;

} // namespace
