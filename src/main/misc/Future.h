/**
 * This file declares the future of an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Future.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_FUTURE_H_
#define MAIN_MISC_FUTURE_H_

#include <exception>
#include <functional>
#include <memory>
#include <thread>

namespace hycast {

template<class R> class FutureImpl;
template<class R> class ExecutorImpl;
template<class R> class BasicExecutorImpl;
template<class R> class BasicCompleterImpl;

/**
 * Future of an asynchronous task with a non-void result.
 * @tparam R  Task's result
 */
template<class R>
class Future final
{
    friend class BasicExecutorImpl<R>;

    std::shared_ptr<FutureImpl<R>> pImpl;

    /**
     * Executes the associated task.
     */
    void operator()() const;
    /**
     * Returns the POSIX thread identifier.
     * @return  POSIX thread identifier
     */
    pthread_t getThreadId() const;
public:
    /**
     * Constructs from nothing.
     */
    Future();
    /**
     * Constructs from the function to execute.
     * @param[in] func  Function to execute
     */
    explicit Future(std::function<R()> func);
    /**
     * Indicates if this instance has a callable or is empty.
     * @return `true` iff this instance has a callable
     */
    operator bool() const noexcept;
    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance.
     */
    bool operator==(const Future<R>& that);
    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  The other instance
     * @return `true` iff this instance is less than the other
     */
    bool operator<(const Future<R>& that) noexcept;
    /**
     * Cancels the task's thread iff the task hasn't already completed.
     * Idempotent.
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void cancel() const;
    /**
     * Waits for the task to complete. Idempotent.
     */
    void wait() const;
    /**
     * Indicates if the task's thread was cancelled. Blocks until the task
     * completes if necessary. Should always be called before getResult().
     * @retval `true`  iff the task's thread was cancelled.
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    bool wasCancelled() const;
    /**
     * Returns the result of the asynchronous task. Blocks until the task
     * completes if necessary. If the task threw an exception, then this
     * function will rethrow it.
     * @return the result of the asynchronous task
     * @throws std::logic_error if the task's thread was cancelled
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    R getResult() const;
};

/**
 * Future of an asynchronous task with a void result. Total specialization of
 * the `Future` class template.
 */
template<>
class Future<void> final
{
    friend class BasicExecutorImpl<void>;

    std::shared_ptr<FutureImpl<void>> pImpl;

    /**
     * Executes the associated task.
     */
    void operator()() const;
    /**
     * Returns the POSIX thread identifier.
     * @return  POSIX thread identifier
     */
    pthread_t getThreadId() const;

public:
    /**
     * Constructs from nothing.
     */
    Future();
    /**
     * Constructs from the function to execute.
     * @param[in] fund  Function to execute
     */
    explicit Future(std::function<void()> func);
    /**
     * Indicates if this instance has a callable or is empty.
     * @return `true` iff this instance has a callable
     */
    operator bool() const noexcept;
    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance.
     */
    bool operator==(const Future<void>& that);
    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  The other instance
     * @return `true` iff this instance is less than the other
     */
    bool operator<(const Future<void>& that) noexcept;
    /**
     * Cancels the task's thread iff the task hasn't already completed.
     * Idempotent.
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void cancel() const;
    /**
     * Waits for the task to complete. Idempotent.
     */
    void wait() const;
    /**
     * Indicates if the task's thread was cancelled. Blocks until the task
     * completes if necessary. Should always be called before getResult().
     * @retval `true`  iff the task's thread was cancelled.
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    bool wasCancelled() const;
    /**
     * Returns the result of the asynchronous task. Blocks until the task
     * completes if necessary. If the task threw an exception, then this
     * function will rethrow it.
     * @return the result of the asynchronous task
     * @throws std::logic_error if the task's thread was cancelled
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void getResult() const;
};

} // namespace

#endif /* MAIN_MISC_FUTURE_H_ */
