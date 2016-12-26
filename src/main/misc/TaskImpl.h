/**
 * This file declares the implementation of an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: TaskImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_TASKIMPL_H_
#define MAIN_MISC_TASKIMPL_H_

#include <condition_variable>
#include <exception>
#include <mutex>

namespace hycast {

/**
 * Abstract base class for generic tasks.
 * @tparam Res  Type of result of task execution
 */
template<class Res>
class BasicTask
{
protected:
    typedef std::function<Res()> Fn;

    /// Function to call to execute task
    Fn                        func;
    /// Exception thrown during task execution
    std::exception_ptr        exceptPtr;
    /// Mutex for guarding task state
    std::mutex                mutex;
    /// Condition variable for notifying of changes in state
    std::condition_variable   cond;
    /// Whether or not task has completed
    bool                      completed;
    /// Whether or not task's thread was cancelled
    bool                      cancelled;

    /**
     * Configures a task as having been cancelled.
     * @param basicTask  Task to be configured as having been cancelled
     */
    static void setCancelled(void* basicTask);
    /**
     * Waits for the task to complete. Doesn't return until it does.
     */
    void wait();
public:
    /**
     * Constructs from a function to call.
     * @param[in] fn  Function to call
     */
    explicit BasicTask(const Fn& func);
    /**
     * Move constructs from a function to call.
     * @param[in] fn  Function to call
     */
    explicit BasicTask(const Fn&& func);
    /**
     * Destroys.
     */
    virtual ~BasicTask();

    // Every task is unique
    BasicTask(BasicTask& task) =delete;
    BasicTask(BasicTask&& task) =delete;
    BasicTask& operator=(BasicTask& task) =delete;
    BasicTask& operator=(BasicTask&& task) =delete;

    /**
     * Executes the task.
     */
    virtual void run() =0;
    /**
     * Indicates if the thread on which the task was executing was cancelled.
     * Blocks until the task completes if necessary.
     * @retval `true`  iff the task's thread was cancelled.
     */
    bool wasCancelled();
    /**
     * Returns the result of the task. Blocks until the task completes if
     * necessary. If the task throws an exception, then this function will
     * rethrow it.
     * @return the result of the task
     * @throws std::logic_error if the task's thread was cancelled
     */
    virtual Res getResult() =0;
};

/**
 * Tasks that have a non-void result (partial specialization).
 */
template<class Res>
class TaskImpl final : public BasicTask<Res>
{
    typedef std::function<Res()> Fn;
    Res                          result;
public:
    /**
     * Constructs from a function to call.
     */
    explicit TaskImpl(const Fn& func)
        : BasicTask<Res>::BasicTask(func)
        , result{}
    {}
    /**
     * Move constructs from a function to call.
     * @param[in] fn  Function to call
     */
    explicit TaskImpl(const Fn&& func)
        : BasicTask<Res>::BasicTask(func)
        , result{}
    {}
    /**
     * Executes the task.
     */
    void run();
    /**
     * Returns the result of the task. Blocks until the task completes if
     * necessary. If the task throws an exception, then this function will
     * rethrow it.
     * @return the result of the task
     * @throws std::logic_error if the task's thread was cancelled
     */
    Res getResult();
};

/**
 * Tasks that have a void result (total specialization).
 */
template<>
class TaskImpl<void> final : public BasicTask<void>
{
    typedef std::function<void()> Fn;
public:
    /**
     * Constructs from a function to call.
     */
    explicit TaskImpl(const Fn& func)
        : BasicTask<void>::BasicTask(func)
    {}
    /**
     * Move constructs from a function to call.
     * @param[in] fn  Function to call
     */
    explicit TaskImpl(const Fn&& func)
        : BasicTask<void>::BasicTask(func)
    {}
    /**
     * Executes the task.
     */
    void run();
    /**
     * Blocks until the task completes if necessary. If the task throws an
     * exception, then this function will rethrow it.
     * @throws std::logic_error if the task's thread was cancelled
     */
    void getResult();
};

} // namespace

#endif /* MAIN_MISC_TASKIMPL_H_ */
