/**
 * This file implements the implementation of an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: TaskImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "TaskImpl.h"

#include <pthread.h>

namespace hycast {

template<class Res>
BasicTask<Res>::BasicTask(const Fn& fn)
    : func{fn}
    , exceptPtr{}
    , mutex{}
    , cond{}
    , completed{false}
    , cancelled{false}
{}

template<class Res>
BasicTask<Res>::BasicTask(const Fn&& fn)
    : func{fn}
    , exceptPtr{}
    , mutex{}
    , cond{}
    , completed{false}
    , cancelled{false}
{}

template<class Res>
BasicTask<Res>::~BasicTask()
{}

template<class Res>
void BasicTask<Res>::setCancelled(void* arg)
{
    BasicTask<Res>* task = reinterpret_cast<BasicTask<Res>*>(arg);
    task->cancelled = true;
    task->completed = true;
    task->cond.notify_one();
}

template<class Res>
void TaskImpl<Res>::run()
{
    pthread_cleanup_push(this->setCancelled, this);
    std::lock_guard<decltype(this->mutex)> lock{this->mutex};
    try {
        result = BasicTask<Res>::func();
    }
    catch (const std::exception& e) {
        this->exceptPtr = std::current_exception();
    }
    pthread_cleanup_pop(0);
    this->completed = true;
    this->cond.notify_one();
}

void TaskImpl<void>::run()
{
    pthread_cleanup_push(this->setCancelled, this);
    std::lock_guard<decltype(mutex)> lock{mutex};
    try {
        this->func();
    }
    catch (const std::exception& e) {
        this->exceptPtr = std::current_exception();
    }
    pthread_cleanup_pop(0);
    this->completed = true;
    this->cond.notify_one();
}

template<class Res>
void BasicTask<Res>::wait()
{
    std::unique_lock<decltype(mutex)> lock{mutex};
    while (!completed)
        cond.wait(lock);
}

template<class Res>
bool BasicTask<Res>::wasCancelled()
{
    wait();
    return cancelled;
}

template<class Res>
Res TaskImpl<Res>::getResult()
{
    this->wait();
    if (this->cancelled)
        throw std::logic_error("Asynchronous task was cancelled");
    if (this->exceptPtr)
        std::rethrow_exception(this->exceptPtr);
    return result;
}

void TaskImpl<void>::getResult()
{
    this->wait();
    if (this->cancelled)
        throw std::logic_error("Asynchronous task was cancelled");
    if (exceptPtr)
        std::rethrow_exception(exceptPtr);
}

template class BasicTask<void>;
template class BasicTask<int>;
template class TaskImpl<int>;

} // namespace
