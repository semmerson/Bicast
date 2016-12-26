/**
 * This file implements an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Task.cpp
 * @author: Steven R. Emmerson
 */

#include "Task.h"
#include "TaskImpl.h"

namespace hycast {

template<class Res>
Task<Res>::Task(const Fn& fn)
    : pImpl{new TaskImpl<Res>(fn)}
{}

template<class Res>
Task<Res>::Task(const Fn&& fn)
    : pImpl{new TaskImpl<Res>(fn)}
{}

template<class Res>
void Task<Res>::run()
{
    pImpl->run();
}

template<class Res>
bool Task<Res>::wasCancelled() const
{
    return pImpl->wasCancelled();
}

template<class Res>
Res Task<Res>::getResult()
{
    return pImpl->getResult();
}

template class Task<void>;
template class Task<int>;

} // namespace
