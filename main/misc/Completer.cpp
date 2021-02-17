/**
 * This file declares a completer of asynchronous tasks. Tasks are submitted to
 * a completer and retrieved in the order of their completion.
 *
 *   @file: Completer.cpp
 * @author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Completer.h"
#include "Executor.h"
#include "Future.h"
#include "Thread.h"

#include <assert.h>
#include <condition_variable>
#include <mutex>
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
    typedef std::condition_variable Cond;
    class FutureQueue
    {
        Mutex                    mutex;
        Cond                     cond;
        std::queue<Future<Ret>>  futures;

    public:
        FutureQueue()
            : mutex{}
            , cond{}
            , futures{}
        {}

        void push(Future<Ret> future)
        {
            LockGuard lock{mutex};
            futures.push(future);
            cond.notify_all();
        }

        Future<Ret> pop()
        {
            UniqueLock lock{mutex};
            while (futures.empty()) {
                Canceler canceler{};
                cond.wait(lock);
            }
            auto future = futures.front();
            futures.pop();
            return future;
        }
    };

    /// Executor
    Executor<Ret>            executor;
    /// Queue of futures of completed tasks
    FutureQueue              completedFutures;

    void add() {
        auto future = executor.getFuture();
        completedFutures.push(future);
    }

    static void addFutureToQueue(void* arg) {
        auto impl = static_cast<Impl*>(arg);
        impl->add();
    }

protected:
    /**
     * Wraps a callable in a callable that adds the associated future to the
     * queue of completed futures when the task completes.
     * @param[in] func  Callable to be wrapped
     * @return          Wrapped callable
     */
    std::function<Ret()> getCallable(const std::function<Ret()>& func)
    {
        return [this,func] {
            Ret result;
            THREAD_CLEANUP_PUSH(addFutureToQueue, this);
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
        : executor{}
        , completedFutures{}
    {}

    ~Impl()
    {
        try {
            executor.shutdown(true);
            executor.awaitTermination();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't destroy completer");
        }
    }

    /**
     * Submits a callable for execution. The callable's future will also be
     * returned by take(), eventually.
     * @param[in,out] func  Callable to be executed
     * @return              Callable's future
     * @exceptionsafety     Basic guarantee
     * @threadsafety        Safe
     * @see                 take()
     */
    Future<Ret> submit(const std::function<Ret()>& func) {
        auto callable = getCallable(func);
        auto future = executor.submit(callable);
        return future;
    }

    /**
     * Returns the next completed future. Blocks until one is available.
     * @return the next completed future
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    Future<Ret> take() {
        return completedFutures.pop();
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
        THREAD_CLEANUP_PUSH(addFutureToQueue, this);
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
Future<Ret> Completer<Ret>::take()
{
    return pImpl->take();
}

template class Completer<int>;
template class Completer<void>;

} // namespace
