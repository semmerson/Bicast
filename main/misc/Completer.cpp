/**
 * This file declares a completer of asynchronous tasks. Tasks are submitted to
 * a completer and retrieved in the order of their completion.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Completer.cpp
 * @author: Steven R. Emmerson
 */

#include "Completer.h"
#include "Executor.h"
#include "Future.h"

#include <assert.h>
#include <condition_variable>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <type_traits>
#include <utility>

namespace hycast {

template<class Ret>
class Completer<Ret>::Impl
{
    /// Types:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;

    /// Variables for synchronizing state changes
    std::mutex               mutex;
    std::condition_variable  cond;
    /// Queue of completed tasks
    std::queue<Future<Ret>>  completedTasks;
    /// Executor
    Executor<Ret>            executor;

    void finish() {
        auto threadId = pthread_self();
        auto future = executor.getFuture(threadId);
        LockGuard lock{mutex};
        completedTasks.push(future);
        cond.notify_all();
    }

protected:
    static void finishTask(void* arg) {
        static_cast<Impl*>(arg)->finish();
    }

    /**
     * Wraps a callable in a callable that adds the associated future to the
     * completed-task queue.
     * @param[in] func  Callable to be wrapped
     * @return          Wrapped callable
     */
    std::function<Ret()> getCallable(const std::function<Ret()>& func)
    {
        return [this,func] {
            Ret result{};
            THREAD_CLEANUP_PUSH(finishTask, this);
            result = func();
            THREAD_CLEANUP_POP(true);
            return result;
        };
    }

public:
    /**
     * Constructs from nothing.
     */
    Impl()
        : mutex{}
        , cond{}
        , completedTasks{}
        , executor{}
    {}

    /**
     * Submits a callable for execution. The callable's future will also be
     * returned by get(), eventually.
     * @param[in,out] func  Callable to be executed
     * @return              Callable's future
     * @exceptionsafety     Basic guarantee
     * @threadsafety        Safe
     * @see                 get()
     */
    Future<Ret> submit(const std::function<Ret()>& func) {
        auto callable = getCallable(func);
        LockGuard lock{mutex};
        auto future = executor.submit(callable);
        return future;
    }

    /**
     * Returns the next completed future. Blocks until one is available.
     * @return the next completed future
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    Future<Ret> get() {
        UniqueLock lock{mutex};
        while (completedTasks.empty())
            cond.wait(lock);
        auto future = completedTasks.front();
        completedTasks.pop();
        return future;
    }
};

/******************************************************************************/

/**
 * Wraps a callable in a callable that adds the associated future to the
 * completed-task queue when the task completes. Complete specialization for
 * callables that return void.
 * @param[in] func  Void callable to be wrapped
 * @return          Wrapped callable
 */
template<>
std::function<void()> Completer<void>::Impl::getCallable(
        const std::function<void()>& func) {
    return [this,func] {
        THREAD_CLEANUP_PUSH(finishTask, this);
        func();
        THREAD_CLEANUP_POP(true);
    };
}

/******************************************************************************/

template<class Ret>
Completer<Ret>::Completer()
    : pImpl(new Impl())
{}

template<class Ret>
Completer<Ret>::~Completer()
{}

template<class Ret>
Future<Ret> Completer<Ret>::submit(const std::function<Ret()>& func)
{
    return pImpl->submit(func);
}

template<class Ret>
Future<Ret> Completer<Ret>::get()
{
    return pImpl->get();
}

template class Completer<int>;
template class Completer<void>;

} // namespace
