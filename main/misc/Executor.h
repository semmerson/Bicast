/**
 * This file declares an executor of asynchronous tasks.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Executor.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_EXECUTOR_H_
#define MAIN_MISC_EXECUTOR_H_

#include "Future.h"
#include "Task.h"
#include "Thread.h"

#include <functional>
#include <memory>

namespace hycast {

/**
 * Class template for an executor of type-returning callables
 */
template<class Ret>
class Executor final
{
    typedef Thread::ThreadId ThreadId;

    class                    Impl;
    std::shared_ptr<Impl>    pImpl;

public:
    /**
     * Default constructs.
     */
    Executor();

    /**
     * Destroys. Cancels all active tasks and waits for them to complete.
     */
    ~Executor();

    /**
     * Submits a callable for execution.
     * @param[in,out] func  Task to be executed
     * @return              The task's future
     * @exceptionsafety     Basic guarantee
     * @threadsafety        Safe
     */
    Future<Ret> submit(std::function<Ret()>& func) const;

    /**
     * Submits a callable for execution.
     * @param[in,out] func  Task to be executed
     * @return              The task's future
     * @exceptionsafety     Basic guarantee
     * @threadsafety        Safe
     */
    Future<Ret> submit(std::function<Ret()>&& func) const;

    /**
     * Returns the future corresponding to a thread identifier.
     * @param[in] threadId     Thread identifier
     * @return                 The corresponding future. Will be empty if no such
     *                         future exists.
     * @throw OutOfRange       No such future
     * @exceptionsafety        Strong guarantee
     * @threadsafety           Safe
     */
    Future<Ret> getFuture(const ThreadId threadId) const;

    void shutdown(const bool mayInterrupt = true) const;

    void awaitTermination() const;
};

} // namespace

#endif /* MAIN_MISC_EXECUTOR_H_ */
