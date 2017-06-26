/**
 * This file implements a task that can be executed asynchronously.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Task.cpp
 *  Created on: Jun 1, 2017
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "Task.h"

#include <functional>

namespace hycast {

template<class Ret>
class Task<Ret>::Impl
{
    std::function<Ret()> func;

public:
    Impl()
    {}

    Impl(std::function<Ret()>& func)
        : func{func}
    {}

    /**
     * Executes this task and returns the result.
     * @return        Task result
     * @threadsafety  Incompatible
     */
    Ret operator ()();
};

template<class Ret>
Ret Task<Ret>::Impl::operator ()()
{
    return func();
}

template<>
void Task<void>::Impl::operator ()()
{
    func();
}

/******************************************************************************/

template<class Ret>
Task<Ret>::Task()
    : pImpl{}
{}

template<class Ret>
Task<Ret>::Task(std::function<Ret()>& func)
    : pImpl{new Impl(func)}
{}

template<class Ret>
Task<Ret>::Task(std::function<Ret()>&& func)
    : pImpl{new Impl(func)}
{}

template<class Ret>
Task<Ret>::operator bool() const noexcept
{
    return pImpl.operator bool();
}

template<class Ret>
Ret Task<Ret>::operator()() const
{
    if (!pImpl)
        throw LogicError(__FILE__, __LINE__, "Empty task");
    return pImpl->operator()();
}

template class Task<int>;

/******************************************************************************
 * Total specialization of `operator()()` for task that returns void.
 */

template<>
void Task<void>::operator ()() const
{
    if (!pImpl)
        throw LogicError(__FILE__, __LINE__, "Empty task");
    pImpl->operator()();
}

template class Task<void>;

} // namespace
