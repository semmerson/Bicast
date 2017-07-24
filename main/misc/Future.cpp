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
    bool                            cancelCalled;
    bool                            canceled;
    bool                            haveResult;
    Thread::Id                threadId;
    bool                            haveThreadId;

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

    static void threadWasCanceled(void* arg)
    {
        auto impl = static_cast<Impl*>(arg);
        LockGuard lock{impl->mutex};
        assert(impl->haveThreadId);
        assert(impl->threadId == Thread::getId());
        impl->canceled = true;
        impl->cond.notify_all();
    }

protected:
    /**
     * Default constructs.
     */
    Impl()
        : mutex{}
        , cond{}
        , exception{}
        , cancelCalled{false}
        , canceled{false}
        , haveResult{false}
        , threadId{}
        , haveThreadId{false}
    {}

    /**
     * @pre `mutex` is locked
     * @retval `true`  Iff the associated task is done
     */
    bool isDone() const
    {
        assert(isLocked());
        return haveResult || exception || canceled;
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
        cond.notify_all();
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
            throw LogicError(__FILE__, __LINE__,
                    "Asynchronous task was canceled");
        return; // `haveResult` must be true
    }

public:
    virtual ~Impl()
    {
        cancel(true);
        UniqueLock lock{mutex};
        wait(lock);
    }

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
        cancelCalled = true;
        if (!haveThreadId) {
            canceled = true;
        }
        else if (!isDone() && mayInterrupt) {
            lock.unlock();
            Thread::cancel(threadId);
            lock.lock();
        }
        cond.notify_all();
    }

    /**
     * @threadsafety  Incompatible
     */
    void operator()()
    {
        THREAD_CLEANUP_PUSH(threadWasCanceled, this);
        try {
            UniqueLock lock{mutex};
            if (haveThreadId)
                throw LogicError(__FILE__, __LINE__,
                        "operator() already called");
            threadId = Thread::getId();
            assert(threadId != Thread::Id{});
            haveThreadId = true;
            if (cancelCalled) {
                canceled = true;
            }
            else {
                lock.unlock();
                bool enabled = Thread::enableCancel();
                Thread::testCancel(); // In case destructor called
                //std::cout << "Calling setResult()\n";
                setResult();
                Thread::testCancel(); // In case destructor called
                Thread::disableCancel(); // Disables Thread::testCancel()
                lock.lock();
                haveResult = true;
                Thread::enableCancel(enabled);
            }
            cond.notify_all();
        }
        catch (const std::exception& e) {
            setException();
        }
        THREAD_CLEANUP_POP(false);
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
    Task<Ret>                   task;
    Ret                         result;

    void setResult()
    {
        result = task();
    }

public:
    /**
     * Constructs from the task to be executed.
     * @param[in] task    Task to be executed
     */
    Impl(Task<Ret>&   task)
        : BasicFuture::Impl{}
        , task{task}
        , result{}
    {}

    /**
     * Constructs from the task to be executed and the key under which to store
     * the thread-specific future.
     * @param[in] task    Task to be executed
     * @param[in] future  Associated public future
     */
    Impl(   Task<Ret>&   task,
            Future<Ret>* future)
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
    Task<void>                   task;

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
