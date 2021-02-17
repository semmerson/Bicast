/**
 * This file declares a task that can be executed asynchronously.
 *
 *        File: Task.h
 *  Created on: May 31, 2017
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
