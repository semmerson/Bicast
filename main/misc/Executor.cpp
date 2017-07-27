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
         * Returns the thread that corresponds to a thread identifier.
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
     * Thread cleanup function. Adds the ID of the current thread-of-execution
     * to the set of done threads. Must be executed on the task's thread.
     */
    void completeTask() {
        LockGuard lock{mutex};
        purgeCompletedTasks();
        /*
         * The thread object associated with the current thread-of-execution is
         * not destroyed because that would cause undefined behavior.
         */
        const auto threadId = Thread::getId();
        assert(threadId != ThreadId{});
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
     * Returns a thread object that references a thread-of-execution that will
     * execute a task.
     * @param[in] task     Task to be executed
     * @param[in] barrier  Synchronization barrier. `barrier.wait()` will be
     *                     called just before `future()`.
     * @return             The thread object
     */
    Thread newThread(
            Task<Ret>&       task,
            Thread::Barrier& barrier)
    {
        return Thread{
            [this,&task,&barrier]() mutable {
                THREAD_CLEANUP_PUSH(taskCompleted, this);
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
                    log_what(e, __FILE__, __LINE__, "Couldn't execute task");
                }
                THREAD_CLEANUP_POP(true);
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
#if 1
        UniqueLock lock{mutex};
        if (closed)
            throw LogicError(__FILE__, __LINE__, "Executor is shut down");
        Thread          thread{};
        Thread::Barrier barrier{2};
        ThreadId        threadId;
        auto            future = task.getFuture();
        try {
            thread = newThread(task, barrier);
            threadId = thread.id();
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't create new thread"));
        }
        try {
            barrier.wait();
            tasks.add(threadId, task);
        }
        catch (const std::exception& e) {
            task.cancel();
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't add new future"));
        }
        try {
            // Mustn't reference `thread` after this
            threads.add(std::move(thread));
        }
        catch (const std::exception& e) {
            tasks.erase(threadId);
            task.cancel();
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't add new thread"));
        }
        return future;
#endif
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
     * Submits a task for execution.
     * @param[in] func    Task to be executed
     * @return            Task future
     * @throw LogicError  `shutdown()` has been called
     */
    Future<Ret> submit(std::function<Ret()> func)
    {
        Task<Ret> task{func};
        return submit(task);
    }

#if 0
    /**
     * Submits a task for execution.
     * @param[in] func    Task to be executed
     * @return            Task future
     * @throw LogicError  `shutdown()` has been called
     */
    Future<Ret> submit(std::function<Ret()>&& func)
    {
        Task<Ret> task{func, [this](bool mayIntr){this->stop(mayIntr);}};
        return submit(task);
    }
#endif

    /**
     * Returns the future associated with the current thread.
     * @return                   The associated future
     * @throw std::out_of_range  No such future
     * @exceptionsafety          Strong guarantee
     * @threadsafety             Safe
     */
    Future<Ret> getFuture()
    {
#if 0
        return *futureKey.get();
#else
        LockGuard lock{mutex};
        return tasks.get(Thread::getId()).getFuture();
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
