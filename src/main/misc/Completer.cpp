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

#include "Executor.h"
#include "Future.h"

#include <pthread.h>
#include <queue>
#include <map>
#include <type_traits>
#include <utility>

namespace hycast {

template<class Ret>
class CompleterImpl final
{
    /// Executor
    Executor<Ret>                    executor;
    /// Executing tasks
    std::map<pthread_t, Future<Ret>> activeTasks;
    /// Completed tasks
    std::queue<Future<Ret>>          completedTasks;
    /// Variables for synchronizing state changes
    std::mutex                       mutex;
    std::condition_variable          cond;

    void finishTask() {
        std::lock_guard<decltype(mutex)> lock{mutex};
        pthread_t threadId = pthread_self();
        if (auto iter = activeTasks.find(threadId) != activeTasks.end()) {
            auto future = iter.second;
            completedTasks.push(future);
            activeTasks.erase(threadId);
            cond.notify_one();
        }
    }

    static void finishTask(void* arg) {
        reinterpret_cast<Completer<Ret>*>(arg)->finishTask();
    }

public:
    /**
     * Constructs from nothing.
     */
    CompleterImpl()
        : executor{}
        , activeTasks{}
        , completedTasks{}
        , mutex{}
        , cond{}
    {}
    /**
     * Submits a callable for execution. The callable's future will, eventually,
     * be returned by get().
     * @param[in,out] func  Callable to be executed
     * @return              The callable's future
     * @exceptionsafety     Basic guarantee
     * @threadsafety        Safe
     */
    Future<Ret> submit(const std::function<Ret()>& func) {
        std::lock_guard<decltype(mutex)> lock{mutex};
        Future<Ret> future{};
        future = executor.submit([this,func,future]() mutable {
            try {
                {
                    std::unique_lock<decltype(this->mutex)> lock{this->mutex};
                    pthread_t threadId = pthread_self();
                    while (auto iter = this->activeTasks.find(threadId) ==
                            this->activeTasks.end())
                        this->cond.wait(lock);
                }
                pthread_cleanup_push(finishTask, this);
                func(); // INVARIANT: Here => `future` is in `activeTasks`
                pthread_cleanup_pop(1);
            }
            catch (const std::exception& e) {
                // Task threw an exception
                future.setException();
            }
        });
        activeTasks.insert(future.getThreadId(), future);
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

template class Completer<void>;
template class Completer<int>;

} // namespace
