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

#include <condition_variable>
#include <map>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <type_traits>
#include <utility>

namespace hycast {

template<class Ret>
class BasicCompleterImpl
{
    friend class CompleterImpl<Ret>;

    /**
     * Returns the future for a callable.
     * @param[in] func  Callable to be executed
     * @return          Callable's future
     */
    virtual Future<Ret> getFuture(const std::function<Ret()>& func) =0;

protected:
    /// Executor
    Executor<Ret>                    executor;
    /// Executing tasks
    std::map<pthread_t, Future<Ret>> activeTasks;
    /// Completed tasks
    std::queue<Future<Ret>>          completedTasks;
    /// Variables for synchronizing state changes
    std::mutex                       mutex;
    std::condition_variable          cond;
    bool                             isShutdown;

    void finishTask() {
        std::lock_guard<decltype(mutex)> lock{mutex};
        auto threadId = pthread_self();
        auto iter = activeTasks.find(threadId);
        auto future = iter->second;
        completedTasks.push(future);
        activeTasks.erase(threadId);
        cond.notify_one();
    }
    static void finishTask(void* arg) {
        reinterpret_cast<CompleterImpl<Ret>*>(arg)->finishTask();
    }

public:
    /**
     * Constructs from nothing.
     */
    BasicCompleterImpl()
        : executor{}
        , activeTasks{}
        , completedTasks{}
        , mutex{}
        , cond{}
        , isShutdown{false}
    {}
    virtual ~BasicCompleterImpl()
    {}
    /**
     * Submits a callable for execution. The callable's future will, eventually,
     * be returned by get().
     * @param[in,out] func       Callable to be executed
     * @return                   Callable's future
     * @throws std::logic_error  shutdown() has been called
     * @exceptionsafety          Basic guarantee
     * @threadsafety             Safe
     */
    Future<Ret> submit(const std::function<Ret()>& func) {
        std::lock_guard<decltype(mutex)> lock{mutex};
        if (isShutdown)
            throw std::logic_error("Completer is shut down");
        auto future = getFuture(func);
        executor.submit(future);
        activeTasks.emplace(future.getThreadId(), future);
        cond.notify_all();
        return future;
    }
    /**
     * Returns the next completed future. Blocks until one is available.
     * @return the next completed future
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    Future<Ret> get() {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (completedTasks.empty())
            cond.wait(lock);
        auto future = completedTasks.front();
        completedTasks.pop();
        future.wait(); // Joins thread
        return future;
    }
    /**
     * Shuts down. Cancels all executing tasks. Will not accept further tasks.
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    void shutdown() {
        std::lock_guard<decltype(mutex)> lock{mutex};
        for (auto pair : activeTasks)
            pair.second.cancel();
        isShutdown = true;
    }
};

template<class Ret>
class CompleterImpl final : public BasicCompleterImpl<Ret>
{
    using BasicCompleterImpl<Ret>::finishTask;

    /**
     * Returns the future for a callable.
     * @param[in] func  Callable to be executed
     * @return          Callable's future
     */
    Future<Ret> getFuture(const std::function<Ret()>& func) {
        auto future = Future<Ret>([this,func]() mutable {
                {
                    // Ensure that `future` is in `activeTasks`
                    std::lock_guard<decltype(this->mutex)> lock{this->mutex};
                }
                Ret result{};
                pthread_cleanup_push(BasicCompleterImpl<Ret>::finishTask, this);
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
    CompleterImpl()
        : BasicCompleterImpl<Ret>::BasicCompleterImpl()
    {}
};

template<>
class CompleterImpl<void> final : public BasicCompleterImpl<void>
{
    using BasicCompleterImpl<void>::finishTask;
    using BasicCompleterImpl<void>::mutex;

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
                pthread_cleanup_push(BasicCompleterImpl<void>::finishTask, this);
                func();
                pthread_cleanup_pop(1);
            });
        return future;
    }
public:
    /**
     * Constructs from nothing.
     */
    CompleterImpl()
        : BasicCompleterImpl<void>::BasicCompleterImpl()
    {}
};

template<class Ret>
Completer<Ret>::Completer()
    : pImpl(new CompleterImpl<Ret>())
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

template<class Ret>
void Completer<Ret>::shutdown()
{
    pImpl->shutdown();
}

template class Completer<void>;
template class Completer<int>;

} // namespace
