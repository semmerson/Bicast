/**
 * This file declares the implementation of the future of an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: FutureImpl.h
 * @author: Steven R. Emmerson
 */

#include "Future.h"

#include <bitset>
#include <condition_variable>
#include <errno.h>
#include <exception>
#include <mutex>
#include <pthread.h>
#include <system_error>
#include <utility>

namespace hycast {

/**
 * Abstract base class for the future of an asynchronous task.
 * @tparam R  Task result
 */
template<class R>
class BasicFutureImpl
{
protected:
    friend class BasicExecutorImpl<R>;

    std::function<R()>              func;
    mutable std::mutex              mutex;
    mutable std::condition_variable cond;
    std::exception_ptr              exception;
    pthread_t                       threadId;
    typedef enum {
        THREAD_ID_SET = 0,
        COMPLETED,
        CANCELLED,
        JOINED,
        NUM_FLAGS
    } State;
    std::bitset<NUM_FLAGS>  state;

    /**
     * Constructs from nothing.
     */
    BasicFutureImpl();
    /**
     * Constructs from the function to execute.
     * @param[in] func  Function to execute
     */
    BasicFutureImpl(std::function<R()> func);
    virtual ~BasicFutureImpl();
    /**
     * Handles cancellation of the task's thread.
     */
    void taskCancelled();
    /**
     * Handles cancellation of a task's thread.
     */
    static void taskCancelled(void* arg);
    /**
     * Sets the result of executing the associated task.
     */
    virtual void setResult() =0;
    /**
     * Executes the associated task.
     */
    void execute();
    /**
     * Returns the POSIX thread identifier.
     * @return  POSIX thread identifier
     */
    pthread_t getThreadId() const;
    /**
     * Cancels the task's thread iff the task hasn't already completed.
     * Idempotent.
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void cancel();
    /**
     * Waits for the task to complete. Joins with the task's thread. Idempotent.
     * @throws std::system_error  The task's thread couldn't be joined
     * @exceptionsafety           Basic guarantee
     * @threadsafety              Safe
     */
    void wait();
    /**
     * Indicates if the task's thread was cancelled. Should always be
     * called before getResult() in case the task was cancelled.
     * @return `true` iff the task's thread was cancelled
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    bool wasCancelled();
    /**
     * Gets the result of the asynchronous task. If the task threw an exception,
     * then it is re-thrown here.
     * @return result of the asynchronous task
     * @throws std::logic_error if the task's thread was cancelled
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    virtual R getResult() =0;

public:
    /**
     * Indicates if this instance has a callable or is empty.
     * @return `true` iff this instance has a callable
     */
    operator bool() const noexcept;
};

template<class R>
class FutureImpl : public BasicFutureImpl<R>
{
    friend class Future<R>;

    using BasicFutureImpl<R>::func;
    using BasicFutureImpl<R>::mutex;
    using BasicFutureImpl<R>::exception;
    using BasicFutureImpl<R>::State;
    using BasicFutureImpl<R>::state;
    using BasicFutureImpl<R>::wait;

    R result;

    /**
     * Constructs from nothing.
     */
    FutureImpl();
    /**
     * Constructs from the function to execute.
     * @param[in] func  Function to execute
     */
    explicit FutureImpl(std::function<R()> func);
    /**
     * Sets the result of executing the associated task.
     */
    void setResult();
    /**
     * Returns the result of the asynchronous task. If the task threw an
     * exception, then it is re-thrown by this function.
     * @return                  Result of the asynchronous task
     * @throws std::logic_error The task's thread was cancelled
     * @exceptionsafety         Strong guarantee
     * @threadsafety            Safe
     */
    R getResult();
};

template<>
class FutureImpl<void> : public BasicFutureImpl<void>
{
    friend class Future<void>;

    using BasicFutureImpl<void>::func;
    using BasicFutureImpl<void>::mutex;
    using BasicFutureImpl<void>::exception;
    using BasicFutureImpl<void>::State;
    using BasicFutureImpl<void>::state;
    using BasicFutureImpl<void>::wait;

    /**
     * Constructs from nothing.
     */
    FutureImpl();
    /**
     * Constructs from the function to execute.
     * @param[in] func  Function to execute
     */
    explicit FutureImpl(std::function<void()> func);
    /**
     * Sets the result of executing the associated task.
     */
    void setResult();
    /**
     * Returns the result of the asynchronous task. If the task threw an
     * exception, then it is re-thrown by this function.
     * @return                  Result of the asynchronous task
     * @throws std::logic_error The task's thread was cancelled
     * @exceptionsafety         Strong guarantee
     * @threadsafety            Safe
     */
    void getResult();
};

} // namespace
