/**
 * This file implements a task that can be executed asynchronously.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Task.cpp
 *  Created on: Jun 1, 2017
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "Task.h"
#include "Thread.h"

#include <functional>

namespace hycast {

template<class Ret>
class Task<Ret>::Impl
{
    typedef std::mutex               Mutex;
    typedef std::lock_guard<Mutex>   LockGuard;
    typedef std::unique_lock<Mutex>  UniqueLock;

    Mutex                mutex;
    std::function<Ret()> func;
    Thread::Id           threadId;
    bool                 haveThreadId;
    Future<Ret>          future;
    bool                 cancelCalled;

    static void markFutureCanceled(void* arg)
    {
        auto impl = static_cast<Impl*>(arg);
        impl->future.setCanceled();
    }

    void setResult()
    {
        future.setResult(func());
    }

public:
    Impl()
        : mutex{}
        , func{}
        , threadId{}
        , haveThreadId{false}
        , future{}
        , cancelCalled{false}
    {}

    Impl(std::function<Ret()>& func)
        : mutex{}
        , func{func}
        , threadId{}
        , haveThreadId{false}
        , future{[this](bool mayIntr){cancel(mayIntr);}}
        , cancelCalled{false}
    {}

    Impl(std::function<Ret()>&& func)
        : mutex{}
        , func{func}
        , threadId{}
        , haveThreadId{false}
        , future{[this](bool mayIntr){cancel(mayIntr);}}
        , cancelCalled{false}
    {}

    ~Impl() noexcept
    {
        try {
            assert(future.hasCompleted());
        }
        catch (const std::exception& e) {
            try {
                std::throw_with_nested(RUNTIME_ERROR("Couldn't destroy task"));
            }
            catch (const std::exception& ex) {
                log_error(ex);
            }
        }
    }

    Future<Ret> getFuture() const
    {
        return future;
    }

    /**
     * Executes this task and sets the result in the future.
     * @threadsafety  Incompatible
     */
    void operator()()
    {
        THREAD_CLEANUP_PUSH(markFutureCanceled, this);
        try {
            UniqueLock lock{mutex};
            if (!cancelCalled) {
                if (haveThreadId)
                    throw LOGIC_ERROR("operator() already called");
                threadId = Thread::getId();
                assert(threadId != Thread::Id{});
                haveThreadId = true;
                lock.unlock();
                setResult();
            }
        }
        catch (const std::exception& e) {
            future.setException(std::current_exception());
        }
        THREAD_CLEANUP_POP(false);
    }

    /**
     * The completion of the task's thread-of-execution is asynchronous with
     * respect to this function.
     * @param[in] mayInterrupt  May the task be canceled if it's already
     *                          started?
     * @guarantee               Upon return, the task's future will indicate
     *                          that the task was canceled if `mayInterrupt` is
     *                          `true` or the task hadn't started
     */
    void cancel(const bool mayInterrupt)
    {
        // Might be called (e.g., by `future`) before `threadId` is set
        UniqueLock lock{mutex};
        if (!haveThreadId) {
            // Task hasn't started
            try {
                cancelCalled = true;
                future.setCanceled();
            }
            catch (const std::exception& e) {
                std::throw_with_nested(RUNTIME_ERROR(
                        "Couldn't cancel future"));
            }
        }
        else {
            // Task has started
            if (mayInterrupt) {
                try {
                    lock.unlock();
                    Thread::cancel(threadId);
                    future.setCanceled();
                }
                catch (const std::exception& e) {
                    std::throw_with_nested(RUNTIME_ERROR(
                            "Couldn't cancel thread"));
                }
            }
        }
    }
};

template<>
void Task<void>::Impl::setResult()
{
    func();
    future.setResult();
}

/******************************************************************************/

template<class Ret>
Task<Ret>::Task()
    : pImpl{}
{}

template<class Ret>
Task<Ret>::Task(std::function<Ret()> func)
    : pImpl{new Impl(func)}
{}

template<class Ret>
Task<Ret>::operator bool() const noexcept
{
    return pImpl.operator bool();
}

template<class Ret>
Future<Ret> Task<Ret>::getFuture() const
{
    if (!pImpl)
        throw LOGIC_ERROR("Empty task");
    return pImpl->getFuture();
}

template<class Ret>
void Task<Ret>::operator()() const
{
    if (!pImpl)
        throw LOGIC_ERROR("Empty task");
    pImpl->operator()();
}

template<class Ret>
void Task<Ret>::cancel(const bool mayInterrupt) const
{
    if (!pImpl)
        throw LOGIC_ERROR("Empty task");
    pImpl->cancel(mayInterrupt);
}

template class Task<int>;

template class Task<void>;

} // namespace
