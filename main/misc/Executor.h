/**
 * This file declares an executor of asynchronous tasks.
 *
 *   @file: Executor.h
 * @author: Steven R. Emmerson
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
    typedef Thread::Id       ThreadId;

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
    ~Executor() noexcept;

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
     * Returns the future associated with the current thread.
     * @return                 The associated future. Will be empty if no such
     *                         future exists.
     * @throw OutOfRange       No such future
     * @exceptionsafety        Strong guarantee
     * @threadsafety           Safe
     */
    Future<Ret> getFuture() const;

    /**
     * Shuts down this instance. Callables that have not started will not be
     * started. Upon return, `submit()` will always throw an exception.
     * @param[in] mayInterrupt  Whether or not the thread-of-execution of
     *                          executing callables may be canceled.
     */
    void shutdown(const bool mayInterrupt = true) const;

    /**
     * Waits until all callables have completed, either by being canceled, by
     * throwing an exception, or by returning.
     * @throw LogicError  Iff `shutdown()` wasn't called first
     */
    void awaitTermination() const;
};

} // namespace

#endif /* MAIN_MISC_EXECUTOR_H_ */
