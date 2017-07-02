/**
 * This file declares the future of an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Future.h
 * @author: Steven Ret. Emmerson
 */

#ifndef MAIN_MISC_FUTURE_H_
#define MAIN_MISC_FUTURE_H_

#include "Task.h"
#include "Thread.h"

#include <exception>
#include <functional>
#include <memory>
#include <thread>

namespace hycast {

class BasicFuture
{
    friend class          std::hash<BasicFuture>;

protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs from a pointer to the template subclass implementation.
     * @param[in] ptr  Pointer to template subclass implementation
     */
    BasicFuture(Impl* ptr);

public:
    typedef std::function<void(bool mayInterrupt)> CancelFunc;

    /**
     * Default constructs.
     */
    BasicFuture();

    /**
     * Destroys.
     */
    virtual ~BasicFuture();

    /**
     * Indicates if this instance is associated with a task or is empty.
     * @return `true` iff this instance is associated with a task
     */
    operator bool() const noexcept;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance.
     * @retval `true`  iff this instance equals the other
     */
    bool operator==(const BasicFuture& that) const noexcept;

    /**
     * Indicates if this instance is not equal to another.
     * @param[in] that  The other instance.
     * @retval `true`  iff this instance does not equal the other
     */
    bool operator!=(const BasicFuture& that) const noexcept;

    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  The other instance
     * @return `true` iff this instance is less than the other
     */
    bool operator<(const BasicFuture& that) const noexcept;

    void operator()() const;

    /**
     * Cancels the task's thread if the task hasn't already completed.
     * Idempotent.
     * @param[in] mayInterrupt  Whether or not the task may be interrupted if it
     *                          has already started
     * @exceptionsafety         Strong guarantee
     * @threadsafety            Safe
     */
    void cancel(bool mayInterrupt = true) const;

    /**
     * Indicates if the task's thread was cancelled. Blocks until the task
     * completes if necessary. Should always be called before getResult().
     * @retval `true`   iff the task's thread was cancelled.
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    bool wasCanceled() const;
};

/**
 * Future of an asynchronous task with a non-void result.
 * @tparam Ret  Task's result
 */
template<class Ret>
class Future final : public BasicFuture
{
    class Impl;

public:
    /**
     * Default constructs.
     */
    Future();

    /**
     * Constructs from the task to be executed.
     * @param[in] task  Task to be executed
     */
    Future(Task<Ret>& task);

    /**
     * Returns the result of the asynchronous task. Blocks until the task
     * completes if necessary. If the task threw an exception, then this
     * function will rethrow it.
     * @return                Result of the asynchronous task
     * @throw LogicError      Task's thread was cancelled or Future is empty
     * @throw std::exception  Exception thrown by task
     * @exceptionsafety       Strong guarantee
     * @threadsafety          Safe
     * @see                   wasCanceled()
     */
    Ret getResult() const;
};

/**
 * Future of an asynchronous task with a void result.
 */
template<>
class Future<void> final : public BasicFuture
{
    class Impl;

public:
    /**
     * Default constructs.
     */
    Future();

    /**
     * Constructs from the task to be executed.
     * @param[in] task  Task to be executed
     */
    Future(Task<void>& task);

    /**
     * Returns when the task is done. If the task threw an exception, then this
     * function will rethrow it.
     * @throw LogicError      Task's thread was cancelled or Future is empty
     * @throw std::exception  Exception thrown by task
     * @exceptionsafety       Strong guarantee
     * @threadsafety          Safe
     * @see                   wasCanceled()
     */
    void getResult() const;
};

} // namespace

namespace std {
    template<> struct hash<hycast::BasicFuture>
    {
        size_t operator()(const hycast::BasicFuture& future) const
        {
            return hash<hycast::BasicFuture::Impl*>()(future.pImpl.get());
        }
    };
    template<> struct hash<hycast::Future<void>>
    {
        size_t operator()(const hycast::Future<void>& future) const
        {
            return hash<hycast::BasicFuture>()(future);
        }
    };
}

#endif /* MAIN_MISC_FUTURE_H_ */
