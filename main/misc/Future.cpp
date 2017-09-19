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
#include <unistd.h>
#include <utility>

namespace hycast {

/**
 * Abstract base class for the future of an asynchronous task.
 */
class BasicFuture::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;

    mutable Mutex                   mutex;
    mutable std::condition_variable cond;
    std::exception_ptr              exception;
    bool                            haveResult;
    bool                            canceled;
    Stop                            stop;

    void cantStop(const bool mayInterrupt)
    {
        throw LOGIC_ERROR("No stop function specified");
    }

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
        , haveResult{false}
        , canceled{false}
        , stop{}
    {}

    /**
     * Constructs.
     * @param[in] stop  Function to call to cancel execution
     */
    Impl(Stop& stop)
        : mutex{}
        , cond{}
        , exception{}
        , haveResult{false}
        , canceled{false}
        , stop{stop}
    {}

    /**
     * Constructs.
     * @param[in] stop  Function to call to cancel execution
     */
    Impl(Stop&& stop)
        : mutex{}
        , cond{}
        , exception{}
        , haveResult{false}
        , canceled{false}
        , stop{stop}
    {}

    void markResult()
    {
        LockGuard lock{mutex};
        haveResult = true;
        cond.notify_all();
    }

    /**
     * @pre `mutex` is locked
     * @retval `true`  Iff the associated task is done
     */
    bool isDone() const
    {
        assert(isLocked());
        return haveResult || exception || canceled;
    }

    /**
     * Waits for the task to complete or for cancel() to be called, whichever
     * occurs first. Idempotent.
     * @param[in] lock   Condition variable lock
     * @pre              `lock` is locked
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    void wait(UniqueLock& lock)
    {
        assert(lock.owns_lock());
        while (!isDone()) {
            Canceler canceler{};
            cond.wait(lock);
        }
        //::fprintf(stderr, "haveResult=%d, exception=%d, canceled=%d\n",
                //haveResult, exception ? 1 : 0, canceled);
    }

    void checkResult()
    {
        UniqueLock lock{mutex};
        wait(lock);
        if (exception)
            std::rethrow_exception(exception);
        if (canceled)
            throw LOGIC_ERROR("Future::cancel() was called");
        return; // `haveResult` must be true
    }

public:
    virtual ~Impl() noexcept
    {}

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
        if (!hasCompleted())
            stop(mayInterrupt);
    }

    void setException(const std::exception_ptr ptr)
    {
        LockGuard lock{mutex};
        exception = ptr;
        cond.notify_all();
    }

    /**
     * Sets the exception to be thrown by `getResult()` to the current
     * exception.
     */
    void setException()
    {
        setException(std::current_exception());
    }

    void setCanceled()
    {
        LockGuard lock{mutex};
        canceled = true;
        cond.notify_all();
    }

    /**
     * @retval `true`  Iff the associated task is done
     */
    bool hasCompleted() const
    {
        UniqueLock lock{mutex};
        return isDone();
    }

    void wait()
    {
        UniqueLock lock{mutex};
        wait(lock);
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
        return canceled && !exception;
    }
};

/******************************************************************************/

BasicFuture::BasicFuture()
    : pImpl{}
{}

BasicFuture::BasicFuture(Impl* ptr)
    : pImpl{ptr}
{}

BasicFuture::~BasicFuture()
{
    try {
        pImpl.reset();
    }
    catch (const std::exception& e) {
        try {
            std::throw_with_nested(
                    RUNTIME_ERROR("Couldn't destroy future"));
        }
        catch (const std::exception& ex) {
            log_error(ex);
        }
    }
}

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

void BasicFuture::cancel(bool mayInterrupt) const
{
    if (!pImpl)
        throw LOGIC_ERROR("Future is empty");
    pImpl->cancel(mayInterrupt);
}

void BasicFuture::setCanceled() const
{
    if (!pImpl)
        throw LOGIC_ERROR("Future is empty");
    pImpl->setCanceled();
}

bool BasicFuture::hasCompleted() const
{
    return pImpl ? pImpl->hasCompleted() : true;
}

void BasicFuture::wait() const
{
    if (pImpl)
        pImpl->wait();
}

bool BasicFuture::wasCanceled() const
{
    return pImpl ? pImpl->wasCanceled() : false;
}

void BasicFuture::setException() const
{
    if (!pImpl)
        throw LOGIC_ERROR("Future is empty");
    pImpl->setException();
}

void BasicFuture::setException(const std::exception_ptr& ptr) const
{
    if (!pImpl)
        throw LOGIC_ERROR("Future is empty");
    pImpl->setException(ptr);
}

/******************************************************************************/

template<class Ret>
class Future<Ret>::Impl : public BasicFuture::Impl
{
    Ret result;

public:
    /**
     * Default constructs.
     */
    Impl()
        : BasicFuture::Impl{}
        , result{}
    {}

    /**
     * Constructs from the function to call to cancel execution.
     * @param[in] stop  Function to call to cancel execution
     */
    Impl(Stop& stop)
        : BasicFuture::Impl{stop}
        , result{}
    {}

    /**
     * Constructs from the function to call to cancel execution.
     * @param[in] stop  Function to call to cancel execution
     */
    Impl(Stop&& stop)
        : BasicFuture::Impl{std::forward<Stop>(stop)}
        , result{}
    {}

    void setResult(Ret result)
    {
        this->result = result;
        markResult();
    }

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
Future<Ret>::Future(Stop& stop)
    : BasicFuture{new Impl(stop)}
{}

template<class Ret>
Future<Ret>::Future(Stop&& stop)
    : BasicFuture{new Impl(std::forward<Stop>(stop))}
{}

template<class Ret>
void Future<Ret>::setResult(Ret result) const
{
    if (!pImpl)
        throw LOGIC_ERROR("Empty future");
    return reinterpret_cast<Impl*>(pImpl.get())->setResult(result);
}

template<class Ret>
Ret Future<Ret>::getResult() const
{
    if (!pImpl)
        throw LOGIC_ERROR("Empty future");
    return reinterpret_cast<Impl*>(pImpl.get())->getResult();
}

template class Future<int>;

/******************************************************************************/

class Future<void>::Impl : public BasicFuture::Impl
{
public:
    /**
     * Default constructs.
     */
    Impl()
        : BasicFuture::Impl{}
    {}

    /**
     * Constructs from the function to call to cancel execution.
     * @param[in] stop  Function to call to cancel execution
     */
    Impl(Stop& stop)
        : BasicFuture::Impl{stop}
    {}

    /**
     * Constructs from the function to call to cancel execution.
     * @param[in] stop  Function to call to cancel execution
     */
    Impl(Stop&& stop)
        : BasicFuture::Impl{std::forward<Stop>(stop)}
    {}

    void setResult()
    {
        markResult();
    }

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

Future<void>::Future(Stop& stop)
    : BasicFuture{new Impl(stop)}
{}

Future<void>::Future(Stop&& stop)
    : BasicFuture{new Impl(std::forward<Stop>(stop))}
{}

void Future<void>::setResult() const
{
    if (!pImpl)
        throw LOGIC_ERROR("Empty future");
    reinterpret_cast<Impl*>(pImpl.get())->setResult();
}

void Future<void>::getResult() const
{
    if (!pImpl)
        throw LOGIC_ERROR("Empty future");
    reinterpret_cast<Impl*>(pImpl.get())->getResult();
}

template class Future<void>;

} // namespace
