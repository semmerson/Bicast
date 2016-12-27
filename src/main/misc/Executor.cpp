/**
 * This file implements an executor of asynchronous tasks.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Executor.cpp
 * @author: Steven R. Emmerson
 */

#include "Executor.h"
#include "Future.h"
#include "FutureImpl.h"

#include <errno.h>
#include <exception>
#include <map>
#include <pthread.h>
#include <stdexcept>
#include <system_error>

namespace hycast {

template<class Ret>
class ExecutorImpl final
{
    friend class Executor<Ret>;

    /**
     * Executes a future. Designed to be called by `pthread_create()`.
     * @param[in] arg  Future to be executed
     */
    static void* execute(void* arg) {
        auto future = reinterpret_cast<FutureImpl<Ret>*>(arg);
        future->execute();
        return nullptr;
    }
    /**
     * Submits a future for execution.
     * @param[in,out] future  Future to be executed
     */
    void submit(Future<Ret>& future) {
        pthread_t   threadId;
        int         status = pthread_create(&threadId, nullptr, execute,
                future.pImpl.get());
        if (status)
            throw std::system_error(errno, std::system_category(),
                    "Couldn't create thread");
    }
public:
    /**
     * Submits a callable for execution.
     * @param[in,out] func       Callable to be executed
     * @throws std::system_error A new thread couldn't be created
     * @exceptionsafety          Basic guarantee
     * @threadsafety             Safe
     */
    Future<Ret> submit(const std::function<Ret()>& func) {
        Future<Ret> future{func};
        submit(future);
        return std::move(future);
    }
};

template<class Ret>
Executor<Ret>::Executor()
    : pImpl(new ExecutorImpl<Ret>())
{}

template<class Ret>
Future<Ret> Executor<Ret>::submit(const std::function<Ret()>& func)
{
    return pImpl->submit(func);
}

template<class Ret>
void Executor<Ret>::submit(Future<Ret>& future)
{
    pImpl->submit(future);
}

template class Executor<void>;
template class Executor<int>;

} // namespace
