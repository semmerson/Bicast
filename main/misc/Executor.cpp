/**
 * This file implements an executor of asynchronous tasks.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Executor.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"
#include "Executor.h"
#include "Thread.h"

#include <cassert>
#include <condition_variable>
#include <errno.h>
#include <exception>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace hycast {

template<class Ret>
class Executor<Ret>::Impl final
{
    /// Types
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;
    typedef std::condition_variable Cond;
    typedef Thread::Id              ThreadId;

    /// Queue of thread identifiers.
    class ThreadIdQueue
    {
        typedef std::queue<ThreadId> Queue;
        Queue                        threadIds;

    public:
        ThreadIdQueue()
            : threadIds{}
        {}

        void push(const ThreadId& threadId)
        {
            assert(threadId != ThreadId{});
            threadIds.push(threadId);
        }

        bool empty()
        {
            return threadIds.empty();
        }

        ThreadId pop()
        {
            if (empty())
                throw LogicError(__FILE__, __LINE__, "Queue is empty");
            auto threadId = threadIds.front();
            assert(threadId != ThreadId{});
            threadIds.pop();
            return threadId;
        }
    };

    /// Map from thread identifiers to threads.
    class ThreadMap
    {
        typedef std::map<ThreadId, Thread> Map;

        Map   threads;

    public:
        ThreadMap()
            : threads{}
        {}

        void add(Thread&& thread)
        {
            auto threadId = thread.id();
            assert(threadId != ThreadId{});
            assert(threads.find(threadId) == threads.end());
            threads[threadId] = std::forward<Thread>(thread);
        }

        Thread remove(const ThreadId& threadId)
        {
            assert(threadId != ThreadId{});
            Thread thread = std::move(threads.at(threadId));
            auto n = threads.erase(threadId);
            assert(n == 1);
            return thread;
        }

        bool empty()
        {
            return threads.empty();
        }
    };

    /// Map from thread identifiers to futures.
    class FutureMap
    {
        typedef std::map<ThreadId, Future<Ret>>  Map;

        Map futures;

    public:
        FutureMap()
            : futures{}
        {}

        void add(const ThreadId& threadId, Future<Ret>& future)
        {
            assert(threadId != ThreadId{});
            assert(futures.find(threadId) == futures.end());
            futures[threadId] = future;
        }

        /**
         * Returns the thread that corresponds to a thread identifier.
         * @param[in] threadId       Thread ID
         * @return                   Corresponding thread
         * @throw std::out_of_range  No such thread
         */
        Future<Ret>& get(const ThreadId& threadId)
        {
            assert(threadId != ThreadId{});
            return futures.at(threadId);
        }

        typename Map::size_type size()
        {
            return futures.size();
        }

        typename Map::size_type erase(const ThreadId& threadId)
        {
            assert(threadId != ThreadId{});
            return futures.erase(threadId);
        }

        template<class Func> void forEach(Func func)
        {
            for (auto pair : futures)
                func(pair.second);
        }
    };

    /// Variables for synchronizing state changes
    mutable Mutex                   mutex;
    mutable std::condition_variable cond;

    /// Active jobs
    ThreadMap                       threads;
    FutureMap                       futures;

    /// Completed threads:
    ThreadIdQueue                   doneThreadIds;

    /// Whether or not this instance has been shut down
    bool                            closed;

    /// Key for thread-specific future:
    Thread::PtrKey<Future<Ret>> futureKey;

    /**
     * Indicates whether or not the mutex is locked. Upon return, the state of
     * the mutex is the same as upon entry.
     * @retval `true`    Iff the mutex is locked
     */
    bool isLocked()
    {
        if (!mutex.try_lock())
            return true;
        mutex.unlock();
        return false;
    }

    /**
     * For each thread referenced by `doneThreadIds`, the thread's associated
     * thread object is erased from `threads` -- destroying the thread object.
     * @pre `mutex` is locked
     * @return `true`  Iff `threads` was modified
     */
    bool destroyDoneThreads()
    {
        assert(isLocked());
        const auto currThreadId = Thread::getId();
        const bool modified = !doneThreadIds.empty();
        while (!doneThreadIds.empty()) {
            const auto threadId = doneThreadIds.pop();
            Thread thread = threads.remove(threadId);
            assert(thread.joinable());
            assert(thread.id() != currThreadId);
            assert(thread.id() == threadId || thread.id() == ThreadId{});
            thread.join();
        }
        return modified;
    }

    /**
     * Thread cleanup function. Removes the future of the task being executed on
     * the current thread from the set of active futures and adds the current
     * thread's identifier to the set of done threads. Must be executed on the
     * task's thread.
     */
    void completeTask() {
        LockGuard lock{mutex};
        destroyDoneThreads();
        /*
         * The thread object associated with the current thread-of-execution is
         * not destroyed because that would cause undefined behavior.
         */
        const auto threadId = Thread::getId();
        assert(threadId != ThreadId{});
        const auto n = futures.erase(threadId);
        assert(n == 1);
        doneThreadIds.push(threadId);
        cond.notify_all();
    }

    /**
     * Thread cleanup function. Calls the thread cleanup member function of the
     * passed-in executor implementation.
     * @param[in] arg  Pointer to `Executor::Impl`
     */
    static void taskCompleted(void* arg) {
        auto pImpl = static_cast<Impl*>(arg);
        pImpl->completeTask();
    }

    /**
     * Returns a thread object whose thread will execute a task's future. Adds
     * the future to the `futures` map.
     * @param[in] future   Task's future
     * @param[in] barrier  Synchronization barrier. `barrier.wait()` will be
     *                     called just before `future()`.
     * @return             The thread object
     */
    Thread newThread(
            Future<Ret>&     future,
            Thread::Barrier& barrier)
    {
        return Thread{
            [this,&future,&barrier]() mutable {
                try {
                    /*
                     * Block parent thread until thread-cleanup routine pushed
                     * and until `getFuture()` will find the future.
                     */
                    THREAD_CLEANUP_PUSH(taskCompleted, this);
                    //futureKey.set(&future);
                    barrier.wait();
                    // future.cancel() held pending until future() entered
                    future();
                    THREAD_CLEANUP_POP(true);
                }
                catch (const std::exception& e) {
                    log_what(e, __FILE__, __LINE__, "Couldn't execute future");
                }
            }
        };
    }

    /**
     * Submits a task for execution.
     * @param[in] task    Task to be executed
     * @return            Future of the task
     * @throw LogicError  `shutdown()` has been called
     */
    Future<Ret> submit(Task<Ret>& task)
    {
        UniqueLock lock{mutex};
        if (closed)
            throw LogicError(__FILE__, __LINE__, "Executor is shut down");
        Future<Ret>     future{task};
        Thread          thread{};
        Thread::Barrier barrier{2};
        ThreadId        threadId;
        try {
            thread = newThread(future, barrier);
            threadId = thread.id();
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't create new thread"));
        }
        try {
            barrier.wait();
            futures.add(threadId, future);
        }
        catch (const std::exception& e) {
            future.cancel();
            future.wasCanceled();
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't add new future"));
        }
        try {
            // Mustn't reference `thread` after this
            threads.add(std::move(thread));
        }
        catch (const std::exception& e) {
            futures.erase(threadId);
            future.cancel();
            future.wasCanceled();
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't add new thread"));
        }
        return future;
    }

public:
    /**
     * Constructs.
     */
    Impl()
        : mutex{}
        , cond{}
        , threads{}
        , futures{}
        , doneThreadIds{}
        , closed{false}
        , futureKey{}
    {}

    /**
     * Destroys. Cancels all active jobs and waits for them to complete.
     */
    ~Impl()
    {
        try {
            shutdown(true);
            awaitTermination();
        }
        catch (const std::exception& e) {
            log_what(e);
        }
    }

    /**
     * Submits a task for execution.
     * @param[in] func    Task to be executed
     * @return            Task future
     * @throw LogicError  `shutdown()` has been called
     */
    Future<Ret> submit(std::function<Ret()>& func)
    {
        Task<Ret> task{func};
        return submit(task);
    }

    /**
     * Submits a task for execution.
     * @param[in] func    Task to be executed
     * @return            Task future
     * @throw LogicError  `shutdown()` has been called
     */
    Future<Ret> submit(std::function<Ret()>&& func)
    {
        Task<Ret> task{func};
        return submit(task);
    }

    /**
     * Returns the future associated with the current thread.
     * @return                   The associated future
     * @throw std::out_of_range  No such future
     * @exceptionsafety          Strong guarantee
     * @threadsafety             Safe
     */
    Future<Ret>& getFuture()
    {
#if 0
        return *futureKey.get();
#else
        LockGuard lock{mutex};
        return futures.get(Thread::getId());
#endif
    }

    /**
     * Shuts down this instance. Upon return, `submit()` will always throw an
     * exception. Idempotent.
     * @param[in] mayInterrupt  Whether or not executing tasks may be
     *                          interrupted
     */
    void shutdown(const bool mayInterrupt)
    {
        LockGuard lock{mutex};
        if (!closed) {
            closed = true;
            if (mayInterrupt) {
                futures.forEach([](Future<Ret>& future) {
                    future.cancel(); // Pending until future() entered
                });
            }
        }
    }

    /**
     * Blocks until all tasks have completed.
     * @throw LogicError  `shutdown()` hasn't been called
     */
    void awaitTermination()
    {
        UniqueLock lock{mutex};
        if (!closed)
            throw LogicError(__FILE__, __LINE__,
                    "Executor hasn't been shut down");
        while (!threads.empty()) {
            while (doneThreadIds.empty())
                cond.wait(lock);
            destroyDoneThreads();
        }
    }
};

template<class Ret>
Executor<Ret>::Executor()
    : pImpl{new Impl{}}
{}

template<class Ret>
Executor<Ret>::~Executor()
{}

template<class Ret>
Future<Ret> Executor<Ret>::submit(std::function<Ret()>& func) const
{
    return pImpl->submit(func);
}

template<class Ret>
Future<Ret> Executor<Ret>::submit(std::function<Ret()>&& func) const
{
    return pImpl->submit(func);
}

template<class Ret>
Future<Ret>& Executor<Ret>::getFuture() const
{
    return pImpl->getFuture();
}

template<class Ret>
void Executor<Ret>::shutdown(const bool mayInterrupt) const
{
    pImpl->shutdown(mayInterrupt);
}

template<class Ret>
void Executor<Ret>::awaitTermination() const
{
    pImpl->awaitTermination();
}

template class Executor<int>;
template class Executor<void>;

} // namespace
