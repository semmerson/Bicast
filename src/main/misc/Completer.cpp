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
class BasicCompleterImpl
{
    friend class CompleterImpl<Ret>;

    /**
     * Wraps a callable in a callable that adds the associated future to the
     * completed-task queue.
     * @param[in] func  Callable to be wrapped
     * @return          Wrapped callable
     */
    virtual std::function<Ret()> getCallable(const std::function<Ret()>& func) =0;

protected:
    /// Variables for synchronizing state changes
    std::mutex                       mutex;
    std::condition_variable          cond;
    /// Queue of completed tasks
    std::queue<Future<Ret>>          completedTasks;
    /// Executor
    Executor<Ret>                    executor;
    /**
     * Number of pending tasks (i.e., submitted tasks that aren't in the
     * completion queue).
     */
    unsigned long                    numPendingTasks;

    void finish() {
        auto threadId = pthread_self();
        auto future = executor.getFuture(threadId);
        assert(future);
        std::lock_guard<decltype(mutex)> lock{mutex};
        completedTasks.push(future);
        --numPendingTasks;
        cond.notify_all();
    }
    static void finishTask(void* arg) {
        reinterpret_cast<CompleterImpl<Ret>*>(arg)->finish();
    }

public:
    /**
     * Constructs from nothing.
     */
    BasicCompleterImpl()
        : mutex{}
        , cond{}
        , completedTasks{}
        , executor{}
        , numPendingTasks{0}
    {}

    /**
     * Destroys. Cancels all pending tasks, waits for all tasks to complete,
     * and clears the completion-queue.
     */
    virtual ~BasicCompleterImpl()
    {
        executor.cancel(); // Blocks until all tasks complete
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (numPendingTasks)
            cond.wait(lock);
        while (!completedTasks.empty()) {
            auto future = completedTasks.front();
            completedTasks.pop();
            future.wait(); // Joins thread
        }
    }

    /**
     * Submits a callable for execution. The callable's future will, eventually,
     * be returned by get().
     * @param[in,out] func       Callable to be executed
     * @return                   Callable's future
     * @exceptionsafety          Basic guarantee
     * @threadsafety             Safe
     */
    Future<Ret> submit(const std::function<Ret()>& func) {
        auto callable = getCallable(func);
        std::lock_guard<decltype(mutex)> lock{mutex};
        auto future = executor.submit(callable);
        ++numPendingTasks;
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
};

template<class Ret>
class CompleterImpl final : public BasicCompleterImpl<Ret>
{
    using BasicCompleterImpl<Ret>::finishTask;
    using BasicCompleterImpl<Ret>::mutex;

    /**
     * Wraps a callable in a callable that adds the associated future to the
     * completed-task queue when the task completes.
     * @param[in] func  Callable to be wrapped
     * @return          Wrapped callable
     */
    std::function<Ret()> getCallable(const std::function<Ret()>& func) {
        return [this,func] {
            Ret result{};
            pthread_cleanup_push(finishTask, this);
            result = func();
            pthread_cleanup_pop(1);
            return result;
        };
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
     * Wraps a callable in a callable that adds the associated future to the
     * completed-task queue when the task completes.
     * @param[in] func  Callable to be wrapped
     * @return          Wrapped callable
     */
    std::function<void()> getCallable(const std::function<void()>& func) {
        return [this,func] {
            pthread_cleanup_push(finishTask, this);
            func();
            pthread_cleanup_pop(1);
        };
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

template class Completer<void>;
template class Completer<int>;

} // namespace
