/**
 * This file declares a completer of asynchronous tasks. Tasks are submitted to
 * a completer and retrieved in the order of their completion.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Completer.h
 * @author: Steven R. Emmerson
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
     * be returned by get().
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
    Future<Ret> get();
};

} // namespace

#endif /* MAIN_MISC_COMPLETER_H_ */
