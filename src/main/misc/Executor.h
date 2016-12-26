/**
 * This file declares an executor of of asynchronous tasks.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Executor.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_EXECUTOR_H_
#define MAIN_MISC_EXECUTOR_H_

#include "Future.h"

#include <functional>
#include <memory>

namespace hycast {

template <class Ret>
class ExecutorImpl;

template<class Ret>
class Executor final
{
    std::shared_ptr<ExecutorImpl<Ret>> pImpl;
public:
    /**
     * Constructs from nothing.
     */
    Executor();
    /**
     * Submits a callable for execution.
     * @param[in,out] func  Task to be executed
     * @return              The task's future
     * @exceptionsafety     Basic guarantee
     * @threadsafety        Safe
     */
    Future<Ret> submit(const std::function<Ret()>& func);
};

} // namespace

#endif /* MAIN_MISC_EXECUTOR_H_ */
