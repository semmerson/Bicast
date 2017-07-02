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
#include <stdexcept>
#include <system_error>
#include <utility>

namespace hycast {

template<class Ret>
class Executor<Ret>::Impl final
{
    /**
     * Synchronizes two threads. One will wait until the gate has been opened
     * by the other.
     */
    class Gate final
    {
        /// Types
        typedef std::mutex              Mutex;
        typedef std::unique_lock<Mutex> UniqueLock;

        Mutex                           mutex;
        std::condition_variable         cond;
        bool                            isOpen;

    public:
        Gate()
            : mutex{}
            , cond{}
            , isOpen{false}
        {}
        void wait()
        {
            UniqueLock lock{mutex};
            while (!isOpen)
                cond.wait(lock);
        }
        void open()
        {
            UniqueLock lock{mutex};
            isOpen = true;
            cond.notify_one();
        }
    };

    /// Types
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;
    typedef Thread::ThreadId        ThreadId;

    /// Variables for synchronizing state changes
    mutable Mutex                   mutex;
    mutable std::condition_variable cond;

    /// Active jobs
    std::map<ThreadId, Thread>      threads;
    std::map<ThreadId, Future<Ret>> futures;

    /// Completed threads:
    std::list<ThreadId>             doneThreadIds;

    /// Whether or not this instance has been shut down
    bool                            closed;

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
     */
    void destroyDoneThreads()
    {
        assert(isLocked());
        const auto currThreadId = Thread::getId();
        for (auto threadId : doneThreadIds) {
            Thread& thread = threads[threadId];
            assert(thread.id() != currThreadId);
            assert(thread.id() == threadId || thread.id() == ThreadId{});
            threads.erase(threadId);
        }
        doneThreadIds.clear();
    }

    /**
     * Removes the future of the task being executed on the current thread from
     * the set of active futures and adds the current thread's identifier to
     * the set of done threads. Must be executed on the task's thread.
     */
    void completeTask() {
        LockGuard lock{mutex};
        /*
         * Destroying the thread object associated with the current thread would
         * cause undefined behavior.
         */
        const auto doneThreadId = Thread::getId();
        const auto n = futures.erase(doneThreadId);
        assert(n <= 1);
        doneThreadIds.push_back(doneThreadId);
        cond.notify_one();
    }

    static void taskCompleted(void* arg) {
        static_cast<Impl*>(arg)->completeTask();
    }

    /**
     * Returns a thread object whose thread will execute a task's future.
     * @param[in] future  Task's future
     * @return            The thread object
     */
    Thread newThread(
            Future<Ret>& future,
            Gate&        gate)
    {
        return Thread{
            [this,future,&gate]() {
                try {
                    THREAD_CLEANUP_PUSH(taskCompleted, this);
                    gate.open(); // Block parent thread until cleanup pushed
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
     * Adds a thread object to this instance.
     * @pre               `isLocked()`
     * @param[in] thread  Thread object to be added
     */
    void add(Thread&& thread)
    {
        assert(isLocked());
        const auto threadId = thread.id();
        assert(threads.find(threadId) == threads.end());
        threads[threadId] = std::forward<Thread>(thread);
    }

    /**
     * Adds a task's future to this instance.
     * @pre                 `isLocked()`
     * @param[in] threadId  Associated thread identifier
     * @param[in] future    Task's future to be added
     */
    void add(
            const ThreadId threadId,
            Future<Ret>&   future)
    {
        assert(isLocked());
        assert(futures.find(threadId) == futures.end());
        futures[threadId] = future;
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
        destroyDoneThreads();
        Future<Ret> future{task};
        Thread      thread{};
        Gate        gate{};
        try {
            thread = newThread(future, gate);
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't create new thread"));
        }
        ThreadId threadId;
        try {
            gate.wait();
            threadId = thread.id();
            add(std::move(thread)); // Mustn't reference `thread` after this
        }
        catch (const std::exception& e) {
            future.cancel();
            future.wasCanceled();
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't add new thread"));
        }
        try {
            add(threadId, future);
        }
        catch (const std::exception& e) {
            future.cancel();
            future.wasCanceled();
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't add thread's future"));
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
     * Returns the future corresponding to a thread identifier.
     * @param[in] threadId       Thread identifier
     * @return                   The corresponding future
     * @throw std::out_of_range  No such future
     * @exceptionsafety          Strong guarantee
     * @threadsafety             Safe
     */
    Future<Ret> getFuture(const ThreadId threadId)
    {
        LockGuard lock{mutex};
        return futures.at(threadId);
    }

    /**
     * Shuts down this instance. Upon return, `submit()` will always throw an
     * exception. Idempotent.
     * @param[in] mayInterrupt  Whether or not executing tasks may be
     *                          interrupted
     */
    void shutdown(const bool mayInterrupt)
    {
        {
            LockGuard lock{mutex};
            if (!closed)
                closed = true;
        }
        /*
         * The mutex must be released when `Future::cancel()` is called
         * because `completeTask()` acquires it.
         */
        if (mayInterrupt)
            for (auto pair : futures)
                pair.second.cancel(); // Pending until future() entered
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
        destroyDoneThreads(); // Necessary
        while (!threads.empty()) {
            cond.wait(lock);
            destroyDoneThreads(); // Necessary
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
Future<Ret> Executor<Ret>::getFuture(const ThreadId threadId) const
{
    return pImpl->getFuture(threadId);
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
