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

#if 0
#include "Thread.h"
#endif

#include <exception>
#include <functional>
#include <memory>
#if 0
#include <thread>
#endif

namespace hycast {

class BasicFuture
{
private:
    friend class std::hash<BasicFuture>;

protected:
    typedef std::function<void(const bool mayInterrupt)> Stop;

    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    //static void cantStop(const bool mayInterrupt);
    static Stop cantStop;

    /**
     * Constructs from a pointer to the template subclass implementation.
     * @param[in] ptr   Pointer to template subclass implementation
     */
    BasicFuture(Impl* ptr);

public:
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

    /**
     * Executes the task given to the constructor.
     * @throw LogicError  Function already called
     * @threadsafety      Incompatible
     */
    void operator()() const;

    void setException() const;

    void setException(const std::exception_ptr& ptr) const;

    /**
     * Cancels the task if the task hasn't already completed. Idempotent.
     * @param[in] mayInterrupt  Whether the task may be interrupted if it's
     *                          being executed
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void cancel(const bool mayInterrupt = true) const;

    void setCanceled() const;

    bool hasCompleted() const;

    void wait() const;

    /**
     * Indicates if the task's thread was cancelled. Blocks until the task
     * completes if necessary. Should always be called before getResult() if
     * having that function throw an exception is undesirable.
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
    typedef BasicFuture::Stop Stop;

    /**
     * Default constructs.
     */
    Future();

    /**
     * Constructs from function to call to cancel execution.
     * @param[in] stop  Function to call to cancel execution
     */
    Future(Stop& stop);

    /**
     * Constructs from function to call to cancel execution.
     * @param[in] stop  Function to call to cancel execution
     */
    Future(Stop&& stop);

    void setResult(Ret result) const;

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
    typedef BasicFuture::Stop Stop;

    /**
     * Default constructs.
     */
    Future();

    /**
     * Constructs from function to call to cancel execution.
     * @param[in] stop  Function to call to cancel execution
     */
    explicit Future(Stop& stop);

    /**
     * Constructs from function to call to cancel execution.
     * @param[in] stop  Function to call to cancel execution
     */
    explicit Future(Stop&& stop);

    void setResult() const;

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
