/**
 * This file implements the future of an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Future.cpp
 * @author: Steven R. Emmerson
 */

#include "Future.h"

#include <bitset>
#include <condition_variable>
#include <errno.h>
#include <exception>
#include <mutex>
#include <pthread.h>
#include <system_error>
#include <utility>

namespace hycast {

/**
 * Abstract base class for the future of an asynchronous task.
 * @tparam R  Task result
 */
template<class R>
class BasicFutureImpl
{
protected:
    friend class BasicExecutorImpl<R>;

    std::function<R()>              func;
    mutable std::mutex              mutex;
    mutable std::condition_variable cond;
    std::exception_ptr              exception;
    pthread_t                       threadId;
    typedef enum {
        THREAD_ID_SET = 0,
        COMPLETED,
        CANCELLED,
        JOINED,
        NUM_FLAGS
    } State;
    std::bitset<NUM_FLAGS>  state;

    /**
     * Constructs from nothing.
     */
    BasicFutureImpl()
        : func{}
        , mutex{}
        , cond{}
        , exception{}
        , threadId{}
        , state{}
    {}
    /**
     * Constructs from the function to execute.
     * @param[in] func  Function to execute
     */
    BasicFutureImpl(std::function<R()> func)
        : func{func}
        , mutex{}
        , cond{}
        , exception{}
        , threadId{}
        , state{}
    {}
    virtual ~BasicFutureImpl()
    {}
    /**
     * Handles cancellation of the task's thread.
     */
    void taskCancelled() {
        std::lock_guard<decltype(mutex)> lock(mutex);
        state[State::CANCELLED] = true;
        state[State::COMPLETED] = true;
        cond.notify_one();
    }
    /**
     * Handles cancellation of a task's thread.
     */
    static void taskCancelled(void* arg) {
        auto future = reinterpret_cast<BasicFutureImpl<R>*>(arg);
        future->taskCancelled();
    }
    /**
     * Sets the result of executing the associated task.
     */
    virtual void setResult() =0;
    /**
     * Executes the associated task.
     */
    void operator()() {
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
    /**
     * Returns the POSIX thread identifier.
     * @return  POSIX thread identifier
     */
    pthread_t getThreadId() const
    {
        std::unique_lock<decltype(mutex)> lock(mutex);
        while (!state[State::THREAD_ID_SET])
            cond.wait(lock);
        return threadId;
    }
    /**
     * Cancels the task's thread iff the task hasn't already completed.
     * Idempotent.
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void cancel() {
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
    /**
     * Waits for the task to complete. Joins with the task's thread. Idempotent.
     * @throws std::system_error  The task's thread couldn't be joined
     * @exceptionsafety           Basic guarantee
     * @threadsafety              Safe
     */
    void wait() {
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
    /**
     * Indicates if the task's thread was cancelled. Should always be
     * called before getResult() in case the task was cancelled.
     * @return `true` iff the task's thread was cancelled
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    bool wasCancelled() {
        wait();
        return state[State::CANCELLED];
    }
    /**
     * Gets the result of the asynchronous task. If the task threw an exception,
     * then it is re-thrown here.
     * @return result of the asynchronous task
     * @throws std::logic_error if the task's thread was cancelled
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    virtual R getResult() =0;

public:
    /**
     * Indicates if this instance has a callable or is empty.
     * @return `true` iff this instance has a callable
     */
    operator bool() const noexcept
    {
        return func.operator bool();
    }
};

template<class R>
class FutureImpl : public BasicFutureImpl<R>
{
    friend class Future<R>;

    using BasicFutureImpl<R>::func;
    using BasicFutureImpl<R>::mutex;
    using BasicFutureImpl<R>::exception;
    using BasicFutureImpl<R>::State;
    using BasicFutureImpl<R>::state;
    using BasicFutureImpl<R>::wait;

    R result;

    /**
     * Constructs from nothing.
     */
    FutureImpl()
        : result{}
    {}
    /**
     * Constructs from the function to execute.
     * @param[in] func  Function to execute
     */
    explicit FutureImpl(std::function<R()> func)
        : BasicFutureImpl<R>::BasicFutureImpl(func)
        , result{}
    {}
    /**
     * Sets the result of executing the associated task.
     */
    void setResult() {
        result = func();
    }
    /**
     * Returns the result of the asynchronous task. If the task threw an
     * exception, then it is re-thrown by this function.
     * @return                  Result of the asynchronous task
     * @throws std::logic_error The task's thread was cancelled
     * @exceptionsafety         Strong guarantee
     * @threadsafety            Safe
     */
    R getResult() {
        wait();
        if (exception)
            std::rethrow_exception(exception);
        if (state[BasicFutureImpl<R>::State::CANCELLED])
            throw std::logic_error("Asynchronous task was cancelled");
        return result;
    }
};

template<>
class FutureImpl<void> : public BasicFutureImpl<void>
{
    friend class Future<void>;

    using BasicFutureImpl<void>::func;
    using BasicFutureImpl<void>::mutex;
    using BasicFutureImpl<void>::exception;
    using BasicFutureImpl<void>::State;
    using BasicFutureImpl<void>::state;
    using BasicFutureImpl<void>::wait;

    /**
     * Constructs from nothing.
     */
    FutureImpl()
        : BasicFutureImpl<void>::BasicFutureImpl()
    {}
    /**
     * Constructs from the function to execute.
     * @param[in] func  Function to execute
     */
    explicit FutureImpl(std::function<void()> func)
        : BasicFutureImpl<void>::BasicFutureImpl(func)
    {}
    /**
     * Sets the result of executing the associated task.
     */
    void setResult() {
        func();
    }
    /**
     * Returns the result of the asynchronous task. If the task threw an
     * exception, then it is re-thrown by this function.
     * @return                  Result of the asynchronous task
     * @throws std::logic_error The task's thread was cancelled
     * @exceptionsafety         Strong guarantee
     * @threadsafety            Safe
     */
    void getResult() {
        wait();
        if (exception)
            std::rethrow_exception(exception);
        if (state[State::CANCELLED])
            throw std::logic_error("Asynchronous task was cancelled");
    }
};

template class BasicFutureImpl<int>;
template class BasicFutureImpl<void>;

template class FutureImpl<int>;
template class FutureImpl<void>;

template<class R>
Future<R>::Future()
    : pImpl()
{}

template<class R>
Future<R>::Future(std::function<R()> func)
    : pImpl(new FutureImpl<R>(func))
{}

template<class R>
Future<R>::operator bool() const noexcept
{
    return pImpl->operator bool();
}

template<class R>
bool Future<R>::operator==(const Future<R>& that) const noexcept
{
    return pImpl == that.pImpl;
}

template<class R>
bool Future<R>::operator<(const Future<R>& that) const noexcept
{
    return pImpl < that.pImpl;
}

template<class R>
void Future<R>::operator()() const
{
    pImpl->operator()();
}

template<class R>
pthread_t Future<R>::getThreadId() const
{
    return pImpl->getThreadId();
}

template<class R>
void Future<R>::cancel() const
{
    pImpl->cancel();
}

template<class R>
void Future<R>::wait() const
{
    pImpl->wait();
}

template<class R>
bool Future<R>::wasCancelled() const
{
    return pImpl->wasCancelled();
}

template<class R>
R Future<R>::getResult() const
{
    return pImpl->getResult();
}

Future<void>::Future()
    : pImpl()
{}

Future<void>::Future(std::function<void()> func)
    : pImpl(new FutureImpl<void>(func))
{}

Future<void>::operator bool() const noexcept
{
    return pImpl->operator bool();
}

bool Future<void>::operator==(const Future<void>& that) const noexcept
{
    return pImpl == that.pImpl;
}

bool Future<void>::operator<(const Future<void>& that) const noexcept
{
    return pImpl < that.pImpl;
}

void Future<void>::operator()() const
{
    pImpl->operator()();
}

pthread_t Future<void>::getThreadId() const
{
    return pImpl->getThreadId();
}

void Future<void>::cancel() const
{
    pImpl->cancel();
}

void Future<void>::wait() const
{
    pImpl->wait();
}

bool Future<void>::wasCancelled() const
{
    return pImpl->wasCancelled();
}

void Future<void>::getResult() const
{
    pImpl->getResult();
}

//template class Future<>;
template class Future<int>;
template class Future<void>;

} // namespace
