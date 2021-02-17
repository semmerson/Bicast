/**
 * This file declares a completer of asynchronous tasks. Tasks are submitted to
 * a completer and retrieved in the order of their completion.
 *
 *   @file: Completer.h
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

#ifndef MAIN_MISC_COMPLETER_H_
#define MAIN_MISC_COMPLETER_H_

#include "Future.h"

#include <functional>
#include <memory>

namespace hycast {

template<class Ret>
class Completer final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs from nothing.
     */
    Completer();

    /**
     * Destroys. Cancels all pending tasks, waits for all tasks to complete,
     * and clears the completion-queue.
     */
    ~Completer();

    /**
     * Submits a callable for execution. The callable's future will, eventually,
     * be returned by take().
     * @param[in,out] func       Task to be executed
     * @return                   Task's future
     * @throws std::logic_error  cancel() has been called
     * @exceptionsafety          Basic guarantee
     * @threadsafety             Safe
     * @throws std::logic_error  Instance is shut down
     */
    Future<Ret> submit(const std::function<Ret()>& func);

    /**
     * Returns the next completed future. Blocks until one is available.
     * @return the next completed future
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    Future<Ret> take();
};

} // namespace

#endif /* MAIN_MISC_COMPLETER_H_ */
