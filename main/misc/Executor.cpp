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

        bool empty() const
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

    /// Completed threads:
    ThreadIdQueue                   doneThreadIds;

    /// Whether or not this instance has been shut down
    bool                            closed;

    /// Key for thread-specific future:
    Thread::PtrKey<Future<Ret>> futureKey;

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
     * Purges completed tasks. For each thread referenced by `doneThreadIds`:
     * - The thread-of-execution is joined;
     * - The associated thread object is destroyed; and
     * - The associated task is destroyed.
     * @pre `mutex` is locked
     * @return `true`  Iff `threads` was modified
     */
    bool purgeCompletedTasks()
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
            tasks.erase(threadId);
        }
        return modified;
    }

    /**
     * Purges tasks that previously completed and adds the task associated with
     * the current thread-of-execution. Must be executed on that task's thread.
     */
    void purgeDoneTasksAndAddDoneTask() {
        LockGuard lock{mutex};
        purgeCompletedTasks();
        /*
         * The thread object associated with the current thread-of-execution is
         * not destroyed because that causes undefined behavior.
         */
        const auto threadId = Thread::getId();
        assert(threadId != ThreadId{});
        doneThreadIds.push(threadId);
        cond.notify_all();
    }

    /**
     * Thread cleanup function. Purges tasks that previously completed and adds
     * the task associated with the current thread-of-execution. Must be
     * executed on that task's thread.
     * @param[in] arg  Pointer to `Executor::Impl`
     */
    static void purgeDoneTasksAndAddDoneTask(void* arg) {
        auto impl = static_cast<Impl*>(arg);
        impl->purgeDoneTasksAndAddDoneTask();
    }

    /**
     * Returns a thread object that references a thread-of-execution that will
     * execute a task.
     * @param[in] task     Task to be executed. Copied.
     * @param[in] barrier  Synchronization barrier. `barrier.wait()` will be
     *                     called just before `task()`.
     * @return             The thread object
     */
    Thread newThread(
            Task<Ret>&       task,
            Thread::Barrier& barrier)
    {
        return Thread{
            [this,task,&barrier]() mutable {
                THREAD_CLEANUP_PUSH(purgeDoneTasksAndAddDoneTask, this);
                try {
                    /*
                     * Block parent thread until thread-cleanup routine pushed
                     * and until `getFuture()` will find the future.
                     */
                    //futureKey.set(&future);
                    barrier.wait();
                    // task.cancel() held pending until task() entered
                    task();
                }
                catch (const std::exception& e) {
                    log_what(e, __FILE__, __LINE__, "Task threw an exception");
                }
                THREAD_CLEANUP_POP(true);
            }
        };
    }

    /**
     * Submits a task for execution. Creates a new thread-of-execution for the
     * task, adds the task to the set of active tasks, and adds the thread to
     * the set of active threads.
     * @param[in] task         Task to be executed. Must not be empty.
     * @return                 Future of the task
     * @throw InvalidArgument  `task` is empty
     * @throw LogicError       `shutdown()` has been called
     */
    Future<Ret> submitTask(Task<Ret>& task)
    {
        if (!task)
            throw InvalidArgument(__FILE__, __LINE__, "Empty task");
        LockGuard lock{mutex};
        if (closed)
            throw LogicError(__FILE__, __LINE__, "Executor is shut down");
        Thread::Barrier barrier{2};
        auto            thread = newThread(task, barrier); // RAII object
        barrier.wait();
        ThreadId threadId = thread.id();
        tasks.add(threadId, task);
        try {
            threads.add(thread);
        }
        catch (const std::exception& e) {
            tasks.erase(threadId);
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't add new thread"));
        }
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
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
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
            throw LogicError(__FILE__, __LINE__,
                    "Executor hasn't been shut down");
        while (!threads.empty()) {
            while (doneThreadIds.empty())
                cond.wait(lock);
            purgeCompletedTasks();
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
