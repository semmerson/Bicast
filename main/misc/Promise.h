/**
 * This file declares the shared-state between an asynchronous task and its
 * future.
 *
 *        File: Promise.h
 *  Created on: Jun 1, 2017
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
