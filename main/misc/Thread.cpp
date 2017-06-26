/**
 * This file implements a task that can be executed asynchronously.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Thread.cpp
 *  Created on: May 11, 2017
 *      Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "Thread.h"

#include <atomic>
#include <functional>
#include <pthread.h>
#include <thread>
#include <utility>

namespace hycast {

Thread::Thread()
    : std::thread{}
    , canceled{false}
    , defaultConstructed{true}
{}

Thread::Thread(Thread&& that)
{
    swap(that);
}

Thread::~Thread()
{
    try {
        if (getId() == native_handle())
            throw LogicError(__FILE__, __LINE__,
                    "Thread object being destroyed on associated thread");
        if (!defaultConstructed) {
            cancel();
            join();
#if 0
            throw LogicError(__FILE__, __LINE__,
                    "Joinable thread object being destroyed");
#endif
        }
    }
    catch (const std::exception& e){
        log_what(e);
    }
}

void Thread::threadCanceled(void* arg)
{
    auto thread = static_cast<Thread*>(arg);
    thread->canceled = true;
}

void Thread::swap(Thread& that) noexcept
{
    log_log("Thread::swap(&) entered");
    std::thread::swap(that);
    bool tmp = that.canceled;
    that.canceled = static_cast<bool>(canceled);
    canceled = tmp;
    tmp = that.defaultConstructed;
    that.defaultConstructed = static_cast<bool>(defaultConstructed);
    defaultConstructed = tmp;
}

Thread& Thread::operator=(Thread&& rhs) noexcept
{
    log_log("Thread::operator=(&&) entered");
    if (this != &rhs) {
        if (joinable()) {
            log_what(LogicError(__FILE__, __LINE__, "Calling std::terminate()"));
            std::terminate();
        }
        swap(rhs);
    }
    return *this;
}

bool Thread::enableCancel(const bool enable)
{
    int previous;
    if (::pthread_setcancelstate(
            enable ? PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE, &previous))
        throw SystemError(__FILE__, __LINE__,
                "pthread_setcancelstate() failure: enable=" +
                std::to_string(enable));
    return previous == PTHREAD_CANCEL_ENABLE ? true : false;
}

void Thread::cancel(const ThreadId& threadId)
{
    int status = ::pthread_cancel(threadId);
    if (status)
        throw SystemError(__FILE__, __LINE__, "pthread_cancel() failure",
                status);
}

void Thread::cancel()
{
    if (!defaultConstructed && !canceled.exchange(true)) {
        int status = ::pthread_cancel(native_handle());
        if (status)
            throw SystemError(__FILE__, __LINE__, "pthread_cancel() failure",
                    status);
    }
}

void swap(Thread& a, Thread& b)
{
    a.swap(b);
}

} // namespace

namespace std {
    template<>
    void swap<hycast::Thread>(
            hycast::Thread& a,
            hycast::Thread& b)
    {
        a.swap(b);
    }
}
