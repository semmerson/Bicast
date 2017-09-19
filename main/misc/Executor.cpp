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

    /// Map from thread identifiers to threads.
    class ThreadMap
    {
        typedef std::map<ThreadId, Thread> Map;

        Map   threads;

    public:
        ThreadMap()
            : threads{}
        {}

        void add(Thread& thread)
        {
            auto threadId = thread.id();
            assert(threadId != ThreadId{});
            assert(threads.find(threadId) == threads.end());
            threads[threadId] = thread;
        }

        Thread remove(const ThreadId& threadId)
        {
            assert(threadId != ThreadId{});
            Thread thread{};
            thread = threads.at(threadId);
            auto n = threads.erase(threadId);
            assert(n == 1);
            return thread;
        }

        bool empty()
        {
            return threads.empty();
        }
    };

    /// Map from thread identifiers to tasks.
    class TaskMap
    {
        typedef std::map<ThreadId, Task<Ret>>  Map;

        Map tasks;

    public:
        TaskMap()
            : tasks{}
        {}

        void add(const ThreadId& threadId, Task<Ret>& task)
        {
            assert(threadId != ThreadId{});
            assert(tasks.find(threadId) == tasks.end());
            tasks[threadId] = task;
        }

        /**
         * Returns a reference to the task that corresponds to a thread
         * identifier. Doesn't remove the task.
         * @param[in] threadId       Thread ID
         * @return                   Corresponding thread
         * @throw std::out_of_range  No such thread
         */
        Task<Ret>& get(const ThreadId& threadId)
        {
            assert(threadId != ThreadId{});
            return tasks.at(threadId);
        }

        typename Map::size_type size()
        {
            return tasks.size();
        }

        typename Map::size_type erase(const ThreadId& threadId)
        {
            assert(threadId != ThreadId{});
            return tasks.erase(threadId);
        }

        template<class Func> void forEach(Func func)
        {
            for (auto pair : tasks)
                func(pair.second);
        }
    };

    /// Variables for synchronizing state changes
    mutable Mutex                   mutex;
    mutable std::condition_variable cond;

    /// Active jobs
    ThreadMap                       threads;
    TaskMap                         tasks;

    /// Previously-completed thread:
    Thread::Id                      doneThreadId;
    Thread::Id                      emptyThreadId;

    /// Whether or not this instance has been shut down
    bool                            closed;

    void stop(const bool mayInterrupt)
    {
        auto threadId = Thread::getId();
        auto task = tasks.get(threadId);
        task.cancel(mayInterrupt);
    }

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
     * Purges any previously completed thread. If `doneThreadId` isn't empty,
     * then:
     * - Its thread-of-execution is joined;
     * - The associated thread object is destroyed;
     * - The associated task is destroyed; and
     * - `doneThreadId` is made empty
     * @pre `mutex` is locked
     */
    void purgeDoneThread()
    {
        assert(isLocked());
        const auto currThreadId = Thread::getId();
        if (doneThreadId != emptyThreadId) {
            Thread thread = threads.remove(doneThreadId);
            assert(thread.id() != currThreadId);
            assert(thread.id() == doneThreadId);
            thread.join();
            tasks.erase(doneThreadId);
            doneThreadId = emptyThreadId;
        } // `thread` is destroyed here
    }

    /**
     * Purges any previously completed thread and saves a reference to the
     * current thread-of-execution. Executed on a task's thread.
     */
    void purge() {
        LockGuard lock{mutex};
        purgeDoneThread(); // Eliminates any previous so only need one element
        /*
         * The thread object associated with the current thread-of-execution is
         * not destroyed because that causes undefined behavior.
         */
        const auto threadId = Thread::getId();
        assert(threadId != emptyThreadId);
        doneThreadId = threadId;
        cond.notify_all();
    }

    /**
     * Thread cleanup function. Purges any previously-completed thread and saves
     * a reference to the current thread-of-execution. Executed on a task's
     * thread.
     * @param[in] arg  Pointer to `Executor::Impl`
     */
    static void purgeDoneThreadAndSaveThisThread(void* arg) {
        auto impl = static_cast<Impl*>(arg);
        impl->purge();
    }

    /**
     * Returns a thread object that references a thread-of-execution that
     * executes a task.
     * @param[in] task     Task to be executed. Copied.
     * @param[in] barrier  Synchronization barrier. `barrier.wait()` will be
     *                     called just before `task()`.
     * @return             The thread object
     */
    Thread newThread(
            Task<Ret>& task,
            Barrier&   barrier)
    {
        return Thread{
            [this,task,&barrier]() mutable {
                THREAD_CLEANUP_PUSH(purgeDoneThreadAndSaveThisThread, this);
                try {
                    /*
                     * Block parent thread until thread-cleanup routine pushed
                     * and to ensure visibility of changes to `threads` and
                     * `tasks`, which are accessed on this thread.
                     */
                    barrier.wait();
                    // task.cancel() held pending until task() entered
                    task();
                }
                catch (const std::exception& e) {
                    try {
                        std::throw_with_nested(
                                RUNTIME_ERROR("Task threw an exception"));
                    }
                    catch (const std::exception& ex) {
                        log_error(ex);
                    }
                }
                THREAD_CLEANUP_POP(true);
            }
        };
    }

    /**
     * Submits a task for execution:
     * - Creates a new thread-of-execution for the task;
     * - Adds the task to the set of active tasks; and
     * - Adds the task's thread to the set of active threads.
     * @param[in] task         Task to be executed. Must not be empty.
     * @return                 Future of the task
     * @throw InvalidArgument  `task` is empty
     * @throw LogicError       `shutdown()` has been called
     */
    Future<Ret> submitTask(Task<Ret>& task)
    {
        if (!task)
            throw INVALID_ARGUMENT("Empty task");
        LockGuard lock{mutex};
        if (closed)
            throw LOGIC_ERROR("Executor is shut down");
        purgeDoneThread();
        Barrier barrier{2};
        auto            thread = newThread(task, barrier); // RAII object
        auto            threadId = thread.id();
        tasks.add(threadId, task);
        try {
            threads.add(thread);
        }
        catch (const std::exception& e) {
            tasks.erase(threadId);
            std::throw_with_nested(RUNTIME_ERROR(
                    "Couldn't add new thread"));
        }
        barrier.wait(); // Synchronize `tasks` & `threads` changes with thread
        return task.getFuture();
    }

public:
    /**
     * Constructs.
     */
    Impl()
        : mutex{}
        , cond{}
        , threads{}
        , tasks{}
        , doneThreadId{}
        , emptyThreadId{}
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
            try {
                std::throw_with_nested(
                        RUNTIME_ERROR("Couldn't destroy executor"));
            }
            catch (const std::exception& ex) {
                log_error(ex);
            }
        }
    }

    /**
     * Submits a job for execution.
     * @param[in] func      Job to be executed
     * @return              Job's future
     * @throw LogicError    `shutdown()` has been called
     * @throw RuntimeError  Runtime failure
     */
    Future<Ret> submit(std::function<Ret()> func)
    {
        try {
            Task<Ret> task{func};
            return submitTask(task);
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RUNTIME_ERROR(
                    "Couldn't submit job"));
        }
    }

    /**
     * Returns the future associated with the current thread.
     * @return                   The associated future
     * @throw std::out_of_range  No such future
     * @exceptionsafety          Strong guarantee
     * @threadsafety             Safe
     */
    Future<Ret> getFuture()
    {
        LockGuard lock{mutex};
        return tasks.get(Thread::getId()).getFuture();
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
                tasks.forEach([](Task<Ret>& task) {
                    task.cancel(); // Pending until task() entered
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
            throw LOGIC_ERROR("Executor hasn't been shut down");
        while (!threads.empty()) {
            while (doneThreadId == emptyThreadId) {
                // Thread cancellation must occur from the leaf threads inward
                //Canceler canceler{};
                cond.wait(lock);
            }
            purgeDoneThread();
        }
    }
};

template<class Ret>
Executor<Ret>::Executor()
    : pImpl{new Impl{}}
{}

template<class Ret>
Executor<Ret>::~Executor() noexcept
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
Future<Ret> Executor<Ret>::getFuture() const
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
