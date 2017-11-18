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
#include <iostream>
#include <map>
#include <mutex>
#include <pthread.h>
#include <system_error>
#include <thread>
#include <utility>

namespace hycast {

class Cue::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;
    typedef std::condition_variable Cond;

    Mutex mutex;
    Cond  cond;
    bool  cued;

public:
    Impl()
        : mutex{}
        , cond{}
        , cued{false}
    {}

    void cue()
    {
        LockGuard lock{mutex};
        cued = true;
        cond.notify_all();
    }

    void wait()
    {
        UniqueLock lock(mutex);
        while (!cued) {
            Canceler canceler{};
            cond.wait(lock);
        }
    }
};

hycast::Cue::Cue()
    : pImpl{new Impl()}
{}

void hycast::Cue::cue() const
{
    pImpl->cue();
}

void hycast::Cue::wait() const
{
    pImpl->wait();
}

/******************************************************************************/

Thread::Impl::ThreadMap::ThreadMap()
    : mutex{}
    , threads{}
    , threadIndexes(0, false)
{}

long Thread::Impl::ThreadMap::setAndGetLowestUnusedIndex() noexcept
{
    long size = threadIndexes.size();
    long num;
    for (num = 0; num < size && threadIndexes[num]; ++num)
        ;
    if (num + 1 > threadIndexes.size())
        threadIndexes.resize(num+1);
    threadIndexes[num] = true;
    return num;
}

void Thread::Impl::ThreadMap::add(Impl* const impl)
{
    LockGuard lock{mutex};
    assert(impl->joinable());
    assert(threads.find(impl->id()) == threads.end());
    impl->threadIndex = setAndGetLowestUnusedIndex();
    threads[impl->id()] = impl;
    //std::clog << "Added thread " << impl->id() << std::endl;
}

bool Thread::Impl::ThreadMap::contains(const ThreadId& threadId)
{
    LockGuard lock{mutex};
    return threads.find(threadId) != threads.end();
}

/**
 * Returns the `Thread` corresponding to a thread identifier.
 * @param[in] threadId       Thread identifier
 * @retval    `nullptr`      No such `Thread`
 * @return                   Corresponding thread
 */
Thread::Impl* Thread::Impl::ThreadMap::get(const ThreadId& threadId)
{
    LockGuard lock{mutex};
    auto iter = threads.find(threadId);
    return (iter == threads.end()) ? nullptr : iter->second;
}

Thread::Impl::ThreadMap::Map::size_type
Thread::Impl::ThreadMap::erase(const ThreadId& threadId)
{
    assert(threadId != ThreadId{});
    LockGuard lock{mutex};
    auto iter = threads.find(threadId);
    if (iter == threads.end())
        return 0;
    threadIndexes[iter->second->threadIndex] = false;
    threads.erase(iter);
    return 1;
}

Thread::Impl::ThreadMap::Map::size_type Thread::Impl::ThreadMap::size()
{
    LockGuard lock{mutex};
    return threads.size();
}

/******************************************************************************/

Barrier::Barrier(const int numThreads)
{
    int status = ::pthread_barrier_init(&barrier, nullptr, numThreads);
    if (status)
        throw SYSTEM_ERROR("pthread_barrier_init() failure",
                status);
}

void Barrier::wait()
{
    int status = ::pthread_barrier_wait(&barrier);
    if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
        throw SYSTEM_ERROR("pthread_barrier_wait() failure",
                status);
}

Barrier::~Barrier() noexcept
{
    int status = ::pthread_barrier_destroy(&barrier);
    if (status)
        log_error(SYSTEM_ERROR(
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
        ::abort();
    auto status = ::pthread_cancel(stdThread.native_handle());
    if (status)
        throw SYSTEM_ERROR("Couldn't cancel thread: "
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
        auto threadId = id();
        {
            /*
             * Canceling the current thread causes misbehavior when the current
             * thread is from a nested `Executor`. Thread termination must occur
             * from the leaf threads inward.
             */
            //Canceler canceler{};
            stdThread.join(); // Cancellation point
            // `id()` will now return an invalid identifier
        }
        //std::clog << "Removing thread " << threadId << std::endl;
        const auto n = threads.erase(threadId);
        assert(n == 1);
    }
    catch (const std::system_error& e) {
        /*
         * `std::thread::join()` throws `std::system_error` whose `code() ==
         * `std::errc::invalid_argument` if thread is canceled
         */
        if (e.code() != std::errc::invalid_argument)
            std::throw_with_nested(RUNTIME_ERROR(
                    "std::thread::join() failure"));
        int status;
        {
            Canceler canceler{};
            status = ::pthread_join(stdThread.native_handle(), nullptr);
        }
        if (status)
            throw SYSTEM_ERROR("pthread_join() failure", status);
    }
    catch (const std::exception& e) {
        std::throw_with_nested(RUNTIME_ERROR(
                "std::thread::join() failure"));
    }
}

void Thread::Impl::ensureCompleted()
{
    LockGuard lock{mutex};
    if (!isCompleted()) {
        privateCancel();
        setCompleted();
    }
}

void Thread::Impl::clearBeingJoined(void* const arg) noexcept
{
    auto impl = static_cast<Impl*>(arg);
    LockGuard lock{impl->mutex};
    impl->clearStateBit(State::beingJoined);
}

void Thread::Impl::ensureJoined()
{
    UniqueLock lock{mutex};
    if (!isJoined() && !isBeingJoined()) {
        setBeingJoined();
        THREAD_CLEANUP_PUSH(clearBeingJoined, this);
        {
            /*
             * Unlock because Thread::Impl::setCompletedAndNotify() might be
             * executing
             */
            UnlockGuard unlock{mutex};
            privateJoin(); // Blocks until thread-of-execution terminates
        }
        setJoined();
        THREAD_CLEANUP_POP(false);
    }
    else while (!isJoined()) {
        Canceler canceler{};
        cond.wait(lock);
    }
}

Thread::Impl::~Impl() noexcept
{
    try {
        if (Thread::getId() == id())
            throw LOGIC_ERROR("Thread object being destroyed on associated "
                    "thread");
        ensureCompleted();
        ensureJoined();
    }
    catch (const std::exception& e){
        try {
            std::throw_with_nested(
                    RUNTIME_ERROR("Couldn't destroy thread object"));
        }
        catch (const std::exception& ex) {
            log_error(ex);
        }
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

long Thread::Impl::threadNumber() const noexcept
{
    return threadIndex;
}

long Thread::Impl::getThreadNumber() noexcept
{
    Impl* impl = threads.get(std::this_thread::get_id());
    return impl ? impl->threadIndex : -1;
}

void Thread::Impl::cancel()
{
    ensureCompleted();
}

void Thread::Impl::cancel(const Id& threadId)
{
    Impl* impl = threads.get(threadId);
    if (impl) {
        impl->cancel();
    }
    else {
        throw OUT_OF_RANGE("No such `Thread`");
    }
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
    if (pImpl.get() != rhs.pImpl.get())
        pImpl = rhs.pImpl;
    return *this;
}

Thread& Thread::operator=(Thread&& rhs)
{
    if (pImpl.get() != rhs.pImpl.get())
        pImpl = std::move(rhs.pImpl);
    return *this;
}

Thread::~Thread() noexcept
{
    try {
        pImpl.reset();
    }
    catch (const std::exception& e) {
        try {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't destroy thread"));
        }
        catch (const std::exception& ex) {
            log_error(ex);
        }
    }
}

Thread::Id Thread::getId() noexcept
{
    return Impl::getId();
}

long Thread::getThreadNumber() noexcept
{
    return Impl::getThreadNumber();
}

Thread::Id Thread::id() const
{
    return pImpl ? pImpl->id() : Thread::Id{};
}

long Thread::threadNumber() const noexcept
{
    return pImpl ? pImpl->threadNumber() : -1;
}

bool Thread::enableCancel(const bool enable) noexcept
{
    //std::clog << "Thread " << getId() << ": enableCancel(" << enable << ")\n";
    int  previous;
    ::pthread_setcancelstate(enable ? PTHREAD_CANCEL_ENABLE :
            PTHREAD_CANCEL_DISABLE, &previous);
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
#if 0
        pImpl.reset();
#endif
    }
}

size_t Thread::size()
{
    return Impl::size();
}

} // namespace
