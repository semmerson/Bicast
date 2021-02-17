/**
 * RAII thread.
 *
 *        File: RaiiThread.cpp
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

#include "config.h"

#include "RaiiThread.h"
#include "error.h"

#include <exception>
#include <pthread.h>
#include <thread>

namespace hycast {

class RaiiThread::Impl {
    std::thread  thread;

public:
    template<class Callable, class... Args>
    Impl(Callable&& callable, Args&&... args)
        : thread(callable, std::forward<Args>(args)...)
    {}

    ~Impl() noexcept
    {
        if (::pthread_cancel(thread.native_handle()))
            LOG_ERROR("Couldn't cancel thread");
        try {
            thread.join();
        }
        catch (const std::exception& ex) {
            try {
                std::throw_with_nested(RUNTIME_ERROR("Couldn't join thread"));
            }
            catch (const std::exception& ex) {
                log_error(ex);
            }
        }
    }
};

/******************************************************************************/

RaiiThread::RaiiThread()
    : pImpl{}
{}

template<class Callable, class... Args>
RaiiThread::RaiiThread(Callable&& callable, Args&&... args)
    : pImpl{new Impl(callable, std::forward<Args>(args)...)}
{}

} // namespace
