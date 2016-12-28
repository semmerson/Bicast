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
#include "FutureImpl.h"

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
    friend class ExecutorImpl<void>;

    /// Executing tasks
    std::map<pthread_t, Future<Ret>> activeTasks;
    /// Variables for synchronizing state changes
    std::mutex                       mutex;
    std::condition_variable          cond;
    /// Is this instance shut down?
    bool                             isShutdown;

    /**
     * Returns the future corresponding to a thread identifier.
     * @param[in] threadId  Thread identifier
     * @return              The corresponding future. Will be empty if no such
     *                      future exists.
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    Future<Ret> getFuture(const pthread_t threadId)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        auto iter = activeTasks.find(threadId);
        return iter == activeTasks.end()
                ? Future<Ret>()
                : iter->second;
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
    void finishTask() {
        std::lock_guard<decltype(mutex)> lock{mutex};
        auto threadId = pthread_self();
        activeTasks.erase(threadId);
        cond.notify_one();
    }
    static void finishTask(void* arg) {
        reinterpret_cast<ExecutorImpl<Ret>*>(arg)->finishTask();
    }
    /**
     * Executes a future. Designed to be called by `pthread_create()`.
     * @param[in] arg  Future to be executed
     */
    static void* execute(void* arg) {
        auto future = reinterpret_cast<FutureImpl<Ret>*>(arg);
        future->execute();
        return nullptr;
    }
    /**
     * Starts executing a future.
     * @param[in,out] future  Future to be executed
     * @throws std::logic_error  shutdown() has been called
     * @exceptionsafety          Basic guarantee
     * @threadsafety             Safe
     */
    void start(Future<Ret>& future) {
        std::lock_guard<decltype(mutex)> lock{mutex};
        if (isShutdown)
            throw std::logic_error("Executor is shut down");
        pthread_t   threadId;
        int         status = pthread_create(&threadId, nullptr, execute,
                future.pImpl.get());
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
        , isShutdown{false}
    {}
    virtual ~BasicExecutorImpl()
    {}
    /**
     * Submits a callable for execution.
     * @param[in,out] func       Callable to be executed
     * @throws std::system_error A new thread couldn't be created
     * @exceptionsafety          Basic guarantee
     * @threadsafety             Safe
     */
    Future<Ret> submit(const std::function<Ret()>& func) {
        auto future = getFuture(func);
        start(future);
        activeTasks.emplace(future.getThreadId(), future);
        cond.notify_all();
        return std::move(future);
    }
    /**
     * Shuts down. Cancels all executing tasks. Will not accept further tasks.
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    void shutdownNow() {
        std::lock_guard<decltype(mutex)> lock{mutex};
        for (auto pair : activeTasks)
            pair.second.cancel();
        isShutdown = true;
    }
};

template<class Ret>
class ExecutorImpl : public BasicExecutorImpl<Ret>
{
    using BasicExecutorImpl<Ret>::finishTask;
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
                    // Ensures that `future` is in `activeTasks`
                    std::lock_guard<decltype(mutex)> lock{mutex};
                }
                Ret result{};
                pthread_cleanup_push(BasicExecutorImpl<Ret>::finishTask, this);
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
    using BasicExecutorImpl<void>::finishTask;
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
                    std::lock_guard<decltype(this->mutex)> lock{this->mutex};
                }
                pthread_cleanup_push(BasicExecutorImpl<void>::finishTask, this);
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
void Executor<Ret>::shutdownNow()
{
    pImpl->shutdownNow();
}

template class Executor<void>;
template class Executor<int>;

} // namespace
