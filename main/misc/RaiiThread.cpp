/**
 * RAII thread.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: RaiiThread.cpp
 *  Created on: Jul 27, 2019
 *      Author: Steven R. Emmerson
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
