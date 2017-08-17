/**
 * This file declares a task that can be executed asynchronously.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Task.h
 *  Created on: May 31, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_TASK_H_
#define MAIN_MISC_TASK_H_

#include "Future.h"

#include <functional>
#include <mutex>

namespace hycast {

template<class Ret>
class Task final
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs. `operator bool()` will return `false`.
     */
    Task();

    /**
     * Constructs from the callable to execute.
     * @param[in] func  Callable to execute
     */
    Task(std::function<Ret()> func);

    Future<Ret> getFuture() const;

    /**
     * Indicates if this instance has a callable or is empty.
     * @return `true` iff this instance has a callable
     */
    operator bool() const noexcept;

    /**
     * Executes this task.
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     * @guarantee          The task's future will be set if the task completes
     */
    void operator()() const;

    /**
     * Cancels this task. The completion of the tasks's thread-of-execution is
     * asynchronous with respect to this function.
     * @param[in] mayInterrupt  May the thread on which the task is executing be
     *                          canceled if the task hasn't already started?
     * @guarantee               Upon return, the task's future will indicate
     *                          that the task was canceled if `mayInterrupt` is
     *                          `true` or the task hasn't started
     */
    void cancel(const bool mayInterrupt = true) const;
};

} // namespace

#endif /* MAIN_MISC_TASK_H_ */
