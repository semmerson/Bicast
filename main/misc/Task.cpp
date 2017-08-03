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
        assert(future.hasCompleted());
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
                    throw LogicError(__FILE__, __LINE__,
                            "operator() already called");
                threadId = Thread::getId();
                assert(threadId != Thread::Id{});
                haveThreadId = true;
                lock.unlock();
                bool enabled = Thread::enableCancel();
                setResult();
                Thread::enableCancel(enabled);
            }
        }
        catch (const std::exception& e) {
            future.setException(std::current_exception());
        }
        THREAD_CLEANUP_POP(false);
    }

    void cancel(const bool mayInterrupt)
    {
        // Might be called (e.g., by `future`) before `threadId` is set
        UniqueLock lock{};
        try {
            lock = UniqueLock{mutex};
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't lock mutex"));
        }
        if (haveThreadId) {
            try {
                lock.unlock();
                if (mayInterrupt)
                    Thread::cancel(threadId);
            }
            catch (const std::exception& e) {
                std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                        "Couldn't cancel thread"));
            }
        }
        else {
            try {
                cancelCalled = true;
                lock.unlock();
                future.setCanceled();
            }
            catch (const std::exception& e) {
                std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                        "Couldn't cancel future"));
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

#if 0
template<class Ret>
Task<Ret>::Task(
        std::function<Ret()>&&       func,
        typename Future<Ret>::Stop&& cancel)
    : pImpl{new Impl(std::forward<std::function<Ret()>>(func),
            std::forward<typename Future<Ret>::Stop>(cancel))}
{}
#endif

template<class Ret>
Task<Ret>::operator bool() const noexcept
{
    return pImpl.operator bool();
}

template<class Ret>
Future<Ret> Task<Ret>::getFuture() const
{
    if (!pImpl)
        throw LogicError(__FILE__, __LINE__, "Empty task");
    return pImpl->getFuture();
}

template<class Ret>
void Task<Ret>::operator()() const
{
    if (!pImpl)
        throw LogicError(__FILE__, __LINE__, "Empty task");
    pImpl->operator()();
}

template<class Ret>
void Task<Ret>::cancel(const bool mayInterrupt) const
{
    if (!pImpl)
        throw LogicError(__FILE__, __LINE__, "Empty task");
    pImpl->cancel(mayInterrupt);
}

template class Task<int>;

template class Task<void>;

} // namespace
