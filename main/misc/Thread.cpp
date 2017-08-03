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

void Thread::Impl::setCompleted(void* arg)
{
    auto impl = static_cast<Impl*>(arg);
    LockGuard lock{impl->mutex};
    impl->state.fetch_or(State::completed);
}

void Thread::Impl::privateCancel()
{
    assert(id() != ThreadId{});
    int status = ::pthread_cancel(stdThread.native_handle());
    if (status)
        throw SystemError(__FILE__, __LINE__, "pthread_cancel() failure: "
                "native_handle=" + std::to_string(stdThread.native_handle()),
                status);
}

void Thread::Impl::privateJoin()
{
    assert(id() != ThreadId{});
    try {
        stdThread.join();
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
    assert(id() == ThreadId{});
}

void Thread::Impl::ensureCompleted()
{
    UniqueLock lock{mutex};
    auto       completed = state.fetch_or(State::completed) & State::completed;
    lock.unlock();
    if (!completed)
        privateCancel();
}

void Thread::Impl::ensureJoined()
{
    UniqueLock lock{mutex};
    auto needsJoining = !(state.fetch_or(State::beingJoined) &
            State::beingJoined);
    if (needsJoining) {
        lock.unlock();
        privateJoin();
        assert(id() == ThreadId{});
        lock.lock();
        state.fetch_or(State::joined);
        cond.notify_all();
    }
    else {
        while (!(state.load() & State::joined))
            cond.wait(lock);
    }
}

bool Thread::Impl::isLocked() const
{
    if (!mutex.try_lock())
        return true;
    mutex.unlock();
    return false;
}

Thread::Impl::~Impl()
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

Thread::Id Thread::Impl::id() const noexcept
{
    return stdThread.get_id();
}

void Thread::Impl::cancel()
{
    ensureCompleted();
}

void Thread::Impl::join()
{
    ensureJoined();
}

/******************************************************************************/

Thread::ThreadMap::ThreadMap()
    : mutex{}
    , threads{}
{}

void Thread::ThreadMap::add(Impl* const impl)
{
    LockGuard lock{mutex};
    assert(threads.find(impl->id()) == threads.end());
    threads[impl->id()] = impl;
}

bool Thread::ThreadMap::contains(const Id& threadId)
{
    LockGuard lock{mutex};
    return threads.find(threadId) != threads.end();
}

Thread::Impl* Thread::ThreadMap::get(const Id& threadId)
{
    LockGuard lock{mutex};
    return threads.at(threadId);
}

void Thread::ThreadMap::erase(const Id& threadId)
{
    LockGuard lock{mutex};
    threads.erase(threadId);
}

Thread::ThreadMap::Map::size_type Thread::ThreadMap::size()
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

Thread::ThreadMap Thread::threads;

void Thread::checkInvariants() const
{
    bool join{joinable()};
    Id   ident{id()};
    assert(join == pImpl.operator bool());
    assert(join == (ident != std::thread::id{}));
    assert(join == threads.contains(ident));
}

Thread::Thread()
    : pImpl{}
{}

#if 0
Thread::Thread(const Thread& that)
{
    if (pImpl)
        throw LogicError(__FILE__, __LINE__, "Target is not empty");
    pImpl = that.pImpl;
    checkInvariants();
}
#endif

Thread::Thread(Thread&& that)
{
    if (pImpl)
        throw LogicError(__FILE__, __LINE__, "Target is not empty");
    pImpl = std::move(that.pImpl);
    checkInvariants();
}

Thread& Thread::operator=(const Thread& rhs)
{
    if (pImpl)
        throw LogicError(__FILE__, __LINE__, "Target is not empty");
    pImpl = rhs.pImpl;
    checkInvariants();
    return *this;
}

Thread& Thread::operator=(Thread&& rhs)
{
    if (pImpl)
        throw LogicError(__FILE__, __LINE__, "Target is not empty");
    pImpl = std::move(rhs.pImpl);
    checkInvariants();
    return *this;
}

Thread::~Thread() noexcept
{
    try {
        if (pImpl.unique()) {
            threads.erase(pImpl->id());
            pImpl.reset();
            checkInvariants();
        }
    }
    catch (const std::exception& e) {
        log_what(e, __FILE__, __LINE__, "Couldn't destroy thread");
    }
}

void Thread::add()
{
    assert(pImpl);
    threads.add(pImpl.get());
    checkInvariants();
}

Thread::Id Thread::getId() noexcept

{
    return std::this_thread::get_id();
}

Thread::Id Thread::id() const
{
    return pImpl ? pImpl->id() : std::thread::id{};
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
    if (pImpl) {
        pImpl->cancel();
        checkInvariants();
    }
}

void Thread::cancel(const Id& threadId)
{
    threads.get(threadId)->cancel();
}

bool Thread::joinable() const noexcept
{
    return pImpl.operator bool();
}

void Thread::join()
{
    if (pImpl) {
        threads.erase(pImpl->id());
        pImpl->join();
        pImpl.reset();
        checkInvariants();
    }
}

size_t Thread::size()
{
    return threads.size();
}

} // namespace
