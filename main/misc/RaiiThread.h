/**
 * RAII thread.
 *
 *        File: RaiiThread.h
 *  Created on: Jul 27, 2019
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

#ifndef MAIN_MISC_RAIITHREAD_H_
#define MAIN_MISC_RAIITHREAD_H_

#include <memory>

namespace hycast {

class RaiiThread
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    RaiiThread(Impl* const impl);

public:
    /**
     * Default constructs.
     */
    RaiiThread();

    /**
     * Constructs. The resulting thread will be cancelled just before the
     * thread-object referenced by the implementation is destroyed.
     *
     * @tparam Callable  Callable object
     * @tparam Args      Arguments to callable object
     */
    template<class Callable, class... Args>
    RaiiThread(Callable&& callable, Args&&... args);

    /**
     * Destroys.
     */
    ~RaiiThread() noexcept;
};

} // namespace

#endif /* MAIN_MISC_RAIITHREAD_H_ */
