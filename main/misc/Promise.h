/**
 * This file declares the shared-state between an asynchronous task and its
 * future.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Promise.h
 *  Created on: Jun 1, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_PROMISE_H_
#define MAIN_MISC_PROMISE_H_

#include <memory>

namespace hycast {

class BasicPromise
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    BasicPromise(Impl* ptr);

public:
    /**
     * Default constructs.
     */
    BasicPromise() =default;

    /**
     * Makes the current exception the cause of the task's failure.
     * @threadsafety Safe
     */
    void setException() const;

    /**
     * Marks the task as being canceled.
     * @threadsafety Safe
     */
    void markCanceled() const;

    /**
     * Indicates if the task was canceled. Doesn't block.
     * @retval `true`   Task was canceled
     * @retval `false`  Task was not canceled
     * @threadsafety    Safe
     */
    bool wasCanceled() const;

    /**
     * Indicates if the associated task is done. Doesn't block.
     * @retval `true`  Iff task is done
     * @threadsafety   Safe
     */
    bool isDone() const;

    /**
     * Waits until the task is done.
     * @retval `true`   The task is done
     * @retval `false`  The task is not done
     * @threadsafety    Safe
     */
    void wait() const;
};

template<class Ret>
class Promise final : public BasicPromise
{
    class       Impl;

public:
    /**
     * Default constructs.
     */
    Promise();

    /**
     * Sets the result of the task.
     * @param[in] result  Task result
     * @threadsafety      Safe
     */
    void setResult(Ret result) const;

    /**
     * Returns the result of the task or throws an exception. Blocks until the
     * task is done.
     * @return                Result of task
     * @throw LogicError      Task was canceled
     * @throw std::exception  Exception thrown by task
     * @threadsafety          Safe
     * @see                   wasCanceled()
     */
    Ret getResult() const;
};

template<>
class Promise<void> final : public BasicPromise
{
    class Impl;

public:
    /**
     * Default constructs.
     */
    Promise();

    /**
     * Sets the result of the task.
     * @threadsafety      Safe
     */
    void setResult() const;

    /**
     * Returns when the task is done or throws an exception. Blocks until the
     * task is done.
     * @throw LogicError      Task was canceled
     * @throw std::exception  Exception thrown by task
     * @threadsafety          Safe
     * @see                   wasCanceled()
     */
    void getResult() const;
};

} // namespace

#endif /* MAIN_MISC_PROMISE_H_ */
