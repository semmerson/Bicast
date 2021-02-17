/**
 * This file implements an executor of asynchronous tasks.
 *
 *   @file: Executor.cpp
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

#include "error.h"
#include "Executor.h"
#include "Thread.h"

#include <cassert>
#include <condition_variable>
#include <errno.h>
#include <exception>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
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

    /// Map from thread identifiers to tasks and associated threads.
    class TaskMap
    {
        struct Entry final {
            Thread    thread;
            Task<Ret> task;
            Entry(Thread& thread, Task<Ret>& task)
                : thread{}
                , task{task}
            {
                this->thread = thread;
            }
        };
        typedef std::map<ThreadId, Entry>  Map;

        Map   taskMap;

    public:
        TaskMap()
            : taskMap{}
        {}

        void add(Thread& thread, Task<Ret>& task)
        {
            auto threadId = thread.id();
            assert(threadId != ThreadId{});
            assert(taskMap.find(threadId) == taskMap.end());
            taskMap.emplace(threadId, Entry{thread, task});
        }

        /**
         * Returns the task corresponding to a thread identifier. Doesn't remove
         * the task.
         * @param[in] threadId       Thread ID
         * @return                   Corresponding thread
         * @throw std::out_of_range  No such thread
         */
        Task<Ret> getTask(const ThreadId& threadId)
        {
            assert(threadId != ThreadId{});
            return taskMap.at(threadId).task;
        }

        Thread& getThread()
        {
            return taskMap.rbegin()->second.thread;
        }

        typename Map::size_type size()
        {
            return taskMap.size();
        }

        void erase(const ThreadId& threadId)
        {
            assert(threadId != ThreadId{});
            auto n = taskMap.erase(threadId);
            assert(n == 1);
        }

        void cancelAll()
        {
            for (auto& pair : taskMap) {
                pair.second.task.cancel(); // Pending until task() entered
                //std::cerr << "Thread canceled\n";
            }
        }
    };

    /// Variables for synchronizing state changes
    mutable Mutex mutex;
    mutable Cond  cond;

    /// Active jobs
    TaskMap       tasks;

    /// Previously-completed thread:
    Thread::Id    doneThreadId;
    Thread::Id    emptyThreadId;

    /// Whether or not this instance has been shut down
    bool          closed;

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
     * Removes any previously-completed thread from `tasks` and destroys (i.e.,
     * joins) it. Ensures that `doneThreadId` is empty.
     * @pre     `mutex` is locked
     * @post    `doneThreadId` is empty
     */
    void removeAndDestroyDoneThread()
    {
        assert(isLocked());
        if (doneThreadId != emptyThreadId) {
            // `doneThreadId`'s `Thread` destroyed => joined
            tasks.erase(doneThreadId); // Joins thread
            cond.notify_all();
            //std::cerr << "Thread joined\n";
            doneThreadId = emptyThreadId;
        }
    }

    /**
     * Thread cleanup function. Purges any previously-completed thread and saves
     * a reference to the current thread-of-execution. Ensures that the number
     * of completed but unjoined threads is one at most. Executed on a task's
     * thread.
     * @param[in] arg  Pointer to associated `Executor::Impl`
     */
    static void purgeDoneThreadAndSaveThisThread(void* arg) {
        auto impl = static_cast<Impl*>(arg);
        LockGuard lock{impl->mutex};
        /*
         * The following eliminates any previous thread so storage for only one
         * element is needed
         */
        impl->removeAndDestroyDoneThread();
        /*
         * The thread object associated with the current thread-of-execution
         * is not destroyed because that would cause undefined behavior.
         */
        impl->doneThreadId = Thread::getId();
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
                     * `taskMap`, which are accessed on this thread.
                     */
                    barrier.wait();
                    // task.cancelAll() held pending until task() entered
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
        removeAndDestroyDoneThread();
        Barrier barrier{2};
        auto    thread = newThread(task, barrier); // RAII object
        tasks.add(thread, task);
        cond.notify_all();
        barrier.wait(); // Synchronize `taskMap` changes with thread
        return task.getFuture();
    }

public:
    /**
     * Constructs.
     */
    Impl()
        : mutex{}
        , cond{}
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
        Future<Ret> future{};
        try {
            Task<Ret> task{func};
            future = submitTask(task);
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't submit job"));
        }
        return future;
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
        return tasks.getTask(Thread::getId()).getFuture();
    }

    /**
     * Shuts down this instance. Upon return, `submit()` will always throw an
     * exception. Idempotent.
     * @param[in] mayInterrupt  Whether or not executing taskMap may be
     *                          interrupted
     */
    void shutdown(const bool mayInterrupt)
    {
        LockGuard lock{mutex};
        if (!closed) {
            closed = true;
            if (mayInterrupt)
                tasks.cancelAll();
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
        while (tasks.size() > 1)
            cond.wait(lock);
        // Last `Thread` not destroyed and might not have completed
        if (tasks.size() == 1) {
            // Allow `purge()` to be called on last `Thread`
            UnlockGuard unlock{mutex};
            tasks.getThread().join();
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
