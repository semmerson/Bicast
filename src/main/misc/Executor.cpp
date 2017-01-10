/**
 * This file implements an executor of asynchronous tasks.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Executor.cpp
 * @author: Steven R. Emmerson
 */

#include "Executor.h"
#include "Future.h"

#include <cassert>
#include <condition_variable>
#include <errno.h>
#include <exception>
#include <map>
#include <mutex>
#include <pthread.h>
#include <stdexcept>
#include <system_error>

namespace hycast {

template<class Ret>
class BasicExecutorImpl
{
    friend class Executor<Ret>;
    friend class ExecutorImpl<Ret>;

    /// Executing tasks
    std::map<pthread_t, Future<Ret>> activeTasks;
    /// Variables for synchronizing state changes
    mutable std::mutex               mutex;
    std::condition_variable          cond;
    bool                             isTerminated;

    /**
     * Returns the future corresponding to a thread identifier.
     * @param[in] threadId  Thread identifier
     * @return              The corresponding future. Will be empty if no such
     *                      future exists.
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    Future<Ret> getFuture(const pthread_t threadId) const
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        auto iter = activeTasks.find(threadId);
        return iter == activeTasks.end()
                ? Future<Ret>()
                : iter->second;
    }

    /**
     * Terminates this instance. Cancels all active tasks and waits for them to
     * complete. Idempotent.
     */
    void terminate()
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        if (!isTerminated) {
            for (auto pair : activeTasks)
                pair.second.cancel();
            while (!activeTasks.empty())
                cond.wait(lock);
            isTerminated = true;
        }
    }

protected:
    /**
     * Returns the future for a callable.
     * @param[in] func  Callable to be executed
     * @return          Callable's future
     */
    virtual Future<Ret> getFuture(const std::function<Ret()>& func) =0;

    /**
     * Removes a task from the set of active tasks.
     */
    void completeTask() {
        std::lock_guard<decltype(mutex)> lock{mutex};
        auto threadId = pthread_self();
        auto n = activeTasks.erase(threadId);
        assert(n == 1);
        cond.notify_one();
    }

    static void taskCompleted(void* arg) {
        reinterpret_cast<ExecutorImpl<Ret>*>(arg)->completeTask();
    }

    /**
     * Executes a future. Designed to be called by `pthread_create()`.
     * @param[in] arg  Future to be executed
     */
    static void* execute(void* arg) {
        auto future = reinterpret_cast<Future<Ret>*>(arg);
        future->operator()();
        return nullptr;
    }

    /**
     * Starts executing a future.
     * @param[in,out] future  Future to be executed
     * @exceptionsafety          Basic guarantee
     * @threadsafety             Safe
     */
    void start(Future<Ret>& future) {
        pthread_t   threadId;
        int         status = pthread_create(&threadId, nullptr, execute,
                &future);
        if (status)
            throw std::system_error(errno, std::system_category(),
                    "Couldn't create thread");
    }

public:
    /**
     * Constructs.
     */
    BasicExecutorImpl()
        : activeTasks{}
        , mutex{}
        , cond{}
        , isTerminated{false}
    {}

    /**
     * Destroys. Cancels all active tasks and waits for them to complete.
     */
    virtual ~BasicExecutorImpl()
    {
        terminate();
    }

    /**
     * Submits a callable for execution.
     * @param[in,out] func       Callable to be executed
     * @throws std::system_error A new thread couldn't be created
     * @throws std::logic_error  `cancel()` has been called
     * @exceptionsafety          Basic guarantee
     * @threadsafety             Safe
     */
    Future<Ret> submit(const std::function<Ret()>& func) {
        std::lock_guard<decltype(mutex)> lock{mutex};
        if (isTerminated)
            throw std::logic_error("Executor has been cancelled");
        auto future = getFuture(func);
        start(future);
        ::pthread_t threadId = future.getThreadId();
        activeTasks.emplace(threadId, future);
        return std::move(future);
    }

    /**
     * Cancels this instance. Cancels all active tasks and waits for them to
     * complete. Idempotent.
     */
    void cancel()
    {
        terminate();
    }
};

template<class Ret>
class ExecutorImpl : public BasicExecutorImpl<Ret>
{
    using BasicExecutorImpl<Ret>::taskCompleted;
    using BasicExecutorImpl<Ret>::getFuture;
    using BasicExecutorImpl<Ret>::mutex;

    /**
     * Returns the future for a callable.
     * @param[in] func  Callable to be executed
     * @return          Callable's future
     */
    Future<Ret> getFuture(const std::function<Ret()>& func) {
        auto future = Future<Ret>([this,func]() mutable {
                {
                    // Ensure that `future` is in `activeTasks`
                    std::lock_guard<decltype(mutex)> lock{mutex};
                }
                Ret result{};
                pthread_cleanup_push(taskCompleted, this);
                result = func();
                pthread_cleanup_pop(1);
                return result;
            });
        return future;
    }
public:
    /**
     * Constructs from nothing.
     */
    ExecutorImpl()
        : BasicExecutorImpl<Ret>::BasicExecutorImpl()
    {}
};

template<>
class ExecutorImpl<void> final : public BasicExecutorImpl<void>
{
    using BasicExecutorImpl<void>::taskCompleted;
    using BasicExecutorImpl<void>::getFuture;
    using BasicExecutorImpl<void>::mutex;

    /**
     * Returns the future for a callable.
     * @param[in] func  Callable to be executed
     * @return          Callable's future
     */
    Future<void> getFuture(const std::function<void()>& func) {
        auto future = Future<void>([this,func]() mutable {
                {
                    // Ensure that `future` is in `activeTasks`
                    std::lock_guard<decltype(mutex)> lock{mutex};
                }
                pthread_cleanup_push(taskCompleted, this);
                func();
                pthread_cleanup_pop(1);
            });
        return future;
    }
public:
    /**
     * Constructs from nothing.
     */
    ExecutorImpl()
        : BasicExecutorImpl<void>::BasicExecutorImpl()
    {}
};

template<class Ret>
Executor<Ret>::Executor()
    : pImpl(new ExecutorImpl<Ret>())
{}

template<class Ret>
Executor<Ret>::~Executor()
{}

template<class Ret>
Future<Ret> Executor<Ret>::submit(const std::function<Ret()>& func)
{
    return pImpl->submit(func);
}

template<class Ret>
Future<Ret> Executor<Ret>::getFuture(const pthread_t threadId)
{
    return pImpl->getFuture(threadId);
}

template<class Ret>
void Executor<Ret>::cancel()
{
    return pImpl->cancel();
}

template class Executor<void>;
template class Executor<int>;

} // namespace
