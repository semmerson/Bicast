/**
 * This file declares an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Task.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_TASK_H_
#define MAIN_MISC_TASK_H_

#include <memory>
#include <functional>

namespace hycast {

template<class Res>
class TaskImpl;

template<class Res = void>
class Task final {
    typedef std::function<Res()>   Fn;
    std::shared_ptr<TaskImpl<Res>> pImpl;
public:
    /**
     * Constructs from a function to call.
     * @param[in] fn  Function to call
     */
    explicit Task(const Fn& fn);
    /**
     * Move constructs from a function to call.
     * @param[in] fn  Function to call
     */
    explicit Task(const Fn&& fn);
    /**
     * Executes the task.
     */
    void run();
    /**
     * Indicates if the thread on which the task was executing was cancelled.
     * Blocks until the task completes if necessary.
     * @retval `true`  iff the task's thread was cancelled.
     */
    bool wasCancelled() const;
    /**
     * Returns the result of the task. Blocks until the task completes if
     * necessary. If the task throws an exception, then this function will
     * rethrow it.
     * @return the result of the task
     */
    Res getResult();
};

} // namespace

#endif /* MAIN_MISC_TASK_H_ */
