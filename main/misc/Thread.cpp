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

Thread::Impl::ThreadMap::ThreadMap()
    : mutex{}
    , threads{}
{}

void Thread::Impl::ThreadMap::add(Impl* const impl)
{
    LockGuard lock{mutex};
    assert(impl->joinable());
    assert(threads.find(impl->id()) == threads.end());
    threads[impl->id()] = impl;
}

bool Thread::Impl::ThreadMap::contains(const ThreadId& threadId)
{
    LockGuard lock{mutex};
    return threads.find(threadId) != threads.end();
}

Thread::Impl* Thread::Impl::ThreadMap::get(const ThreadId& threadId)
{
    LockGuard lock{mutex};
    return threads.at(threadId);
}

Thread::Impl::ThreadMap::Map::size_type
Thread::Impl::ThreadMap::erase(const ThreadId& threadId)
{
    assert(threadId != ThreadId{});
    LockGuard lock{mutex};
    const auto n = threads.erase(threadId);
    return n;
}

Thread::Impl::ThreadMap::Map::size_type Thread::Impl::ThreadMap::size()
{
    LockGuard lock{mutex};
    return threads.size();
}

/******************************************************************************/

Thread::Barrier::Barrier(const int numThreads)
{
    int status = ::pthread_barrier_init(&barrier, nullptr, numThreads);
    if (status)
        throw SystemError(__FILE__, __LINE__, "pthread_barrier_init() failure",
                status);
}

void Thread::Barrier::wait()
{
    int status = ::pthread_barrier_wait(&barrier);
    if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
        throw SystemError(__FILE__, __LINE__, "pthread_barrier_wait() failure",
                status);
}

Thread::Barrier::~Barrier() noexcept
{
    int status = ::pthread_barrier_destroy(&barrier);
    if (status)
        log_what(SystemError(__FILE__, __LINE__,
                "pthread_barrier_destroy() failure", status));
}

/******************************************************************************/

Thread::Impl::ThreadMap Thread::Impl::threads;

bool Thread::Impl::isLocked() const
{
    if (!mutex.try_lock())
        return true;
    mutex.unlock();
    return false;
}

bool Thread::Impl::lockedJoinable() const noexcept
{
    assert(isLocked());
    //return stdThread.joinable();
    return !isJoined();
}

void Thread::Impl::setCompletedAndNotify(void* arg)
{
    auto impl = static_cast<Impl*>(arg);
    LockGuard lock{impl->mutex};
    impl->setCompleted();
}

void Thread::Impl::privateCancel()
{
    assert(isLocked());
    assert(lockedJoinable());
    if (native_handle != stdThread.native_handle())
        assert(false);
    int status = ::pthread_cancel(stdThread.native_handle());
    if (status)
        throw SystemError(__FILE__, __LINE__, "Couldn't cancel thread: "
                "native_handle=" + std::to_string(stdThread.native_handle()),
                status);
}

/**
 * @guarantee  Successful return implies joined thread and `size()` returning
 *             one less than before
 */
void Thread::Impl::privateJoin()
{
    assert(joinable());
    try {
        /*
         * Entry in `threads` erased while the identifier returned by `id()` is
         * valid.
         */
        const auto n = threads.erase(id());
        assert(n == 1);
        stdThread.join();
        // `id()` will now return an invalid identifier
    }
    catch (const std::system_error& e) {
        /*
         * `std::thread::join()` throws `std::system_error` whose `code() ==
         * `std::errc::invalid_argument` if thread is canceled
         */
        if (e.code() != std::errc::invalid_argument)
            std::throw_with_nested(RuntimeError{__FILE__, __LINE__,
                    "std::thread::join() failure"});
        auto status = ::pthread_join(stdThread.native_handle(), nullptr);
        if (status)
            throw SystemError(__FILE__, __LINE__, "pthread_join() failure",
                    status);
    }
    catch (const std::exception& e) {
        std::throw_with_nested(RuntimeError{__FILE__, __LINE__,
                "std::thread::join() failure"});
    }
}

void Thread::Impl::ensureCompleted()
{
    UniqueLock lock{mutex};
    if (!isCompleted()) {
        privateCancel();
        setCompleted();
    }
}

void Thread::Impl::ensureJoined()
{
    UniqueLock lock{mutex};
    if (!isBeingJoined()) {
        setBeingJoined();
        /*
         * Unlock because Thread::Impl::setCompletedAndNotify() might be
         * executing
         */
        lock.unlock();
        privateJoin(); // Blocks until thread-of-execution joined
        lock.lock();
        //cond.notify_all();
        setJoined();
    }
    else while (!isJoined()) {
        cond.wait(lock);
    }
}

Thread::Impl::~Impl() noexcept
{
    try {
        if (Thread::getId() == id())
            throw LogicError(__FILE__, __LINE__,
                    "Thread object being destroyed on associated thread");
        ensureCompleted();
        ensureJoined();
    }
    catch (const std::exception& e){
        log_what(e, __FILE__, __LINE__, "Couldn't destroy thread object");
    }
}

Thread::Impl::ThreadId Thread::Impl::id() const noexcept
{
    LockGuard lock{mutex};
    return stdThread.get_id();
}

Thread::Impl::ThreadId Thread::Impl::getId() noexcept
{
    return std::this_thread::get_id();
}

void Thread::Impl::cancel()
{
    ensureCompleted();
}

void Thread::Impl::cancel(const Id& threadId)
{
    threads.get(threadId)->cancel();
}

bool Thread::Impl::joinable() const
{
    LockGuard lock{mutex};
    return lockedJoinable();
}

void Thread::Impl::join()
{
    ensureJoined();
}

size_t Thread::Impl::size()
{
    return threads.size();
}

/******************************************************************************/

Thread::Thread()
    : pImpl{}
{}

#if 0
// See `Thread.h` for why this is commented-out
Thread::Thread(const Thread& that)
{
    if (pImpl)
        throw LogicError(__FILE__, __LINE__, "Target is not empty");
    pImpl = that.pImpl;
    checkInvariants();
}
#endif

Thread::Thread(Thread&& that)
    : pImpl{std::forward<Pimpl>(that.pImpl)}
{}

Thread& Thread::operator=(const Thread& rhs)
{
    if (pImpl)
        throw LogicError(__FILE__, __LINE__, "Target is not empty");
    pImpl = rhs.pImpl;
    return *this;
}

Thread& Thread::operator=(Thread&& rhs)
{
    if (pImpl)
        throw LogicError(__FILE__, __LINE__, "Target is not empty");
    pImpl = std::move(rhs.pImpl);
    return *this;
}

Thread::~Thread() noexcept
{
    try {
        auto enabled = disableCancel();
        pImpl.reset();
        enableCancel(enabled);
    }
    catch (const std::exception& e) {
        log_what(e, __FILE__, __LINE__, "Couldn't destroy thread");
    }
}

Thread::Id Thread::getId() noexcept

{
    return Impl::getId();
}

Thread::Id Thread::id() const
{
    return pImpl ? pImpl->id() : Thread::Id{};
}

bool Thread::enableCancel(const bool enable)
{
    int  previous;
    auto status = ::pthread_setcancelstate(
            enable ? PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE, &previous);
    if (status)
        throw SystemError(__FILE__, __LINE__,
                "pthread_setcancelstate() failure: enable=" +
                std::to_string(enable), status);
    return previous == PTHREAD_CANCEL_ENABLE;
}

void Thread::testCancel()
{
    ::pthread_testcancel();
}

void Thread::cancel()
{
    if (pImpl)
        pImpl->cancel();
}

void Thread::cancel(const Id& threadId)
{
    Impl::cancel(threadId);
}

bool Thread::joinable() const noexcept
{
    return pImpl && pImpl->joinable();
}

void Thread::join()
{
    if (pImpl) {
        pImpl->join();
        pImpl.reset();
    }
}

size_t Thread::size()
{
    return Impl::size();
}

} // namespace
