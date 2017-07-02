/**
 * This file implements a RAII class for an independent thread-of-control that
 * supports copying, assignment, and POSIX thread cancellation.
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
#include <cassert>
#include <functional>
#include <map>
#include <mutex>
#include <pthread.h>
#include <system_error>
#include <thread>
#include <utility>

namespace hycast {

/**
 * Indicates if a mutex is locked by the current thread.
 * @param[in,out] mutex  The mutex
 * @return `true`        Iff the mutex is locked
 */
static bool isLocked(std::mutex& mutex)
{
    if (!mutex.try_lock())
        return true;
    mutex.unlock();
    return false;
}

void Thread::Impl::ensureJoined()
{
    try {
        if (stdThread.joinable())
            stdThread.join();
    }
    catch (const std::system_error& e) {
        /*
         * std::thread::join() throws invalid argument exception if thread
         * canceled
         */
        if (e.code() != std::errc::invalid_argument)
            std::throw_with_nested(RuntimeError{__FILE__, __LINE__,
                    "std::thread::join() failure"});
    }
    catch (const std::exception& e) {
        std::throw_with_nested(RuntimeError{__FILE__, __LINE__,
                "std::thread::join() failure"});
    }
}

void Thread::Impl::privateCancel()
{
    int status = ::pthread_cancel(stdThread.native_handle());
    if (status)
        throw SystemError(__FILE__, __LINE__, "pthread_cancel() failure: "
                "native_handle=" + std::to_string(stdThread.native_handle()),
                status);
}

Thread::Impl::~Impl()
{
    try {
        if (Thread::getId() == id())
            throw LogicError(__FILE__, __LINE__,
                    "Thread object being destroyed on associated thread");
        if (!done)
            privateCancel();
        ensureJoined();
    }
    catch (const std::exception& e){
        log_what(e, __FILE__, __LINE__, "Couldn't destroy thread object");
    }
}

Thread::ThreadId Thread::Impl::id() const
{
    if (stdThread.joinable())
        return stdThread.get_id();
    throw InvalidArgument(__FILE__, __LINE__, "Thread was canceled");
}

void Thread::Impl::cancel()
{
    if (!done.exchange(true))
        privateCancel();
}

void Thread::Impl::join()
{
    ensureJoined();
}

/******************************************************************************/

Thread::Mutex                             Thread::mutex;
std::map<Thread::ThreadId, Thread::Pimpl> Thread::threads;

Thread::Thread()
    : pImpl{}
{}

Thread::Thread(const Thread& that)
    : pImpl{that.pImpl}
{}

Thread::Thread(Thread&& that)
    : pImpl{that.pImpl}
{}

Thread::~Thread()
{
    LockGuard lock{mutex};
    if (pImpl && pImpl.use_count() == 2) // this->pImpl + threads => 2
        threads.erase(id());
}

void Thread::add(Thread& thread)
{
    LockGuard lock{mutex};
    assert(threads.find(thread.id()) == threads.end());
    threads[thread.id()] = thread.pImpl;
}

Thread& Thread::operator=(const Thread& rhs)
{
    if (pImpl)
        this->~Thread();
    pImpl = rhs.pImpl;
}

Thread::ThreadId Thread::getId() noexcept
{
    return std::this_thread::get_id();
}

Thread::ThreadId Thread::id() const
{
    if (!pImpl)
        throw InvalidArgument(__FILE__, __LINE__, "Default-constructed thread");
    return pImpl->id();
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

void Thread::staticCancel(const Pimpl& pImpl)
{
    assert(isLocked(mutex));
    auto ident = pImpl->id();
    pImpl->cancel();
    auto n = threads.erase(ident);
    assert(n == 1);
}

void Thread::cancel() const
{
    if (pImpl) {
        LockGuard lock{mutex};
        staticCancel(pImpl);
    }
}

void Thread::cancel(const ThreadId& threadId)
{
    LockGuard lock{mutex};
    auto& pImpl = threads.at(threadId);
    staticCancel(pImpl);
}

void Thread::join()
{
    if (pImpl)
        pImpl->join();
}

size_t Thread::size()
{
    LockGuard lock{mutex};
    return threads.size();
}

} // namespace
