/**
 * This file implements the future of an asynchronous task.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Future.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "Future.h"
#include "Thread.h"

#include <bitset>
#include <cassert>
#include <condition_variable>
#include <errno.h>
#include <exception>
#include <iostream>
#include <system_error>
#include <utility>

namespace hycast {

/**
 * Abstract base class for the future of an asynchronous task.
 * @tparam R  Task result
 */
class BasicFuture::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;

    mutable Mutex                   mutex;
    mutable std::condition_variable cond;
    std::exception_ptr              exception;
    bool                            canceled;
    bool                            haveResult;
    bool                            haveThreadId;
    Thread::ThreadId                threadId;

    /**
     * Indicates whether or not the mutex is locked. Upon return, the state of
     * the mutex is the same as upon entry.
     * @retval `true`    Iff the mutex is locked
     */
    bool isLocked() const
    {
        if (!mutex.try_lock())
            return true;
        mutex.unlock();
        return false;
    }

protected:
    /**
     * Default constructs.
     */
    Impl()
        : mutex{}
        , cond{}
        , exception{}
        , canceled{false}
        , haveResult{false}
        , haveThreadId{false}
        , threadId{}
    {}

    /**
     * @pre `mutex` is locked
     * @retval `true`  Iff the associated task is done
     */
    bool isDone() const
    {
        assert(isLocked());
        return exception || canceled || haveResult;
    }

    virtual void setResult() =0;

    /**
     * Sets the exception to be thrown by `getResult()` to the current
     * exception.
     */
    void setException()
    {
        LockGuard lock{mutex};
        exception = std::current_exception();
        cond.notify_one();
    }

    /**
     * Waits for the task to complete. Idempotent.
     * @param[in] lock   Condition variable lock
     * @pre              `lock` is locked
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    void wait(UniqueLock& lock)
    {
        assert(lock.owns_lock());
        while (!isDone())
            cond.wait(lock);
    }

    void checkResult()
    {
        UniqueLock lock{mutex};
        wait(lock);
        if (haveResult)
            return;
        if (exception)
            std::rethrow_exception(exception);
        if (canceled)
            throw LogicError(__FILE__, __LINE__,
                    "Asynchronous task was canceled");
    }

public:
    virtual ~Impl() =default;

    /**
     * Cancels the task iff the task hasn't already completed. Idempotent.
     * @param[in] mayInterrupt  Whether or not the thread on which the task is
     *                          executing may be canceled. If false and the task
     *                          has already started, then it will complete
     *                          normally or throw an exception: it's thread will
     *                          not be canceled.
     * @exceptionsafety         Strong guarantee
     * @threadsafety            Safe
     */
    void cancel(const bool mayInterrupt)
    {
        UniqueLock lock{mutex};
        if (!isDone()) {
            canceled = true;
            if (mayInterrupt) {
                if (!haveThreadId)
                    cond.wait(lock);
                Thread::cancel(threadId);
            }
            cond.notify_one();
        }
    }

    /**
     * Indicates if the task completed by being canceled. Blocks until the task
     * completes. Should be called before getResult() if having that function
     * throw an exception is undesirable.
     * @return `true`   Iff the task completed by being canceled
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     * @see             getResult()
     */
    bool wasCanceled()
    {
        UniqueLock lock{mutex};
        wait(lock);
        return canceled;
    }

    void operator()()
    {
        try {
            UniqueLock lock{mutex};
            threadId = Thread::getId();
            haveThreadId = true;
            cond.notify_one();
            if (!canceled) {
                bool enabled = Thread::enableCancel();
                lock.unlock();
                //std::cout << "Calling setResult()\n";
                setResult();
                lock.lock();
                haveResult = true;
                cond.notify_one();
                Thread::enableCancel(enabled);
            }
        }
        catch (const std::exception& e) {
            setException();
        }
    }
};

BasicFuture::BasicFuture()
    : pImpl{}
{}

BasicFuture::BasicFuture(Impl* ptr)
    : pImpl{ptr}
{}

BasicFuture::~BasicFuture()
{}

BasicFuture::operator bool() const noexcept
{
    return pImpl.operator bool();
}

bool BasicFuture::operator==(const BasicFuture& that) const noexcept
{
    return pImpl == that.pImpl;
}

bool BasicFuture::operator!=(const BasicFuture& that) const noexcept
{
    return pImpl != that.pImpl;
}

bool BasicFuture::operator<(const BasicFuture& that) const noexcept
{
    return pImpl < that.pImpl;
}

void BasicFuture::operator()() const
{
    if (!pImpl)
        throw LogicError(__FILE__, __LINE__, "Future is empty");
    pImpl->operator()();
}

void BasicFuture::cancel(bool mayInterrupt) const
{
    if (!pImpl)
        throw LogicError(__FILE__, __LINE__, "Future is empty");
    pImpl->cancel(mayInterrupt);
}

bool BasicFuture::wasCanceled() const
{
    return pImpl ? pImpl->wasCanceled() : false;
}

/******************************************************************************/

template<class Ret>
class Future<Ret>::Impl : public BasicFuture::Impl
{
    Task<Ret> task;
    Ret       result;

    void setResult()
    {
        result = task();
    }

public:
    /**
     * Constructs from the task to be executed.
     * @param[in] task  Task to be executed
     */
    Impl(Task<Ret>& task)
        : BasicFuture::Impl{}
        , task{task}
        , result{}
    {}

    /**
     * Returns the result of the asynchronous task. Blocks until the task is
     * done. If the task threw an exception, then it is re-thrown by this
     * function.
     * @return             Result of the asynchronous task
     * @throws LogicError  The task's thread was canceled
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     * @see                wasCanceled()
     */
    Ret getResult() {
        checkResult();
        return result;
    }
};

template<class Ret>
Future<Ret>::Future()
    : BasicFuture{}
{}

template<class Ret>
Future<Ret>::Future(Task<Ret>& task)
    : BasicFuture{new Impl(task)}
{}

template<class Ret>
Ret Future<Ret>::getResult() const
{
    if (!pImpl)
        throw LogicError(__FILE__, __LINE__, "Empty future");
    return reinterpret_cast<Impl*>(pImpl.get())->getResult();
}

template class Future<int>;

/******************************************************************************/

class Future<void>::Impl : public BasicFuture::Impl
{
    Task<void> task;

    void setResult()
    {
        task();
    }

public:
    /**
     * Constructs from the task to be executed.
     * @param[in] task  Task to be executed
     */
    Impl(Task<void>& task)
        : BasicFuture::Impl{}
        , task{task}
    {}

    /**
     * Returns when the task is done. If the task threw an exception, then it is
     * re-thrown by this function.
     * @throws LogicError  The task was canceled
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     * @see                wasCanceled()
     */
    void getResult() {
        checkResult();
    }
};

Future<void>::Future()
    : BasicFuture{}
{}

Future<void>::Future(Task<void>& task)
    : BasicFuture{new Impl(task)}
{}

void Future<void>::getResult() const
{
    if (!pImpl)
        throw LogicError(__FILE__, __LINE__, "Empty future");
    reinterpret_cast<Impl*>(pImpl.get())->getResult();
}

template class Future<void>;

} // namespace
