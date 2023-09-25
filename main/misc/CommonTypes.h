/**
 * @file: CommonTypes.h
 * @brief: Common types used in the code.
 *
 *  Created on: Aug 10, 2022
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_COMMONTYPES_H_
#define MAIN_MISC_COMMONTYPES_H_

#include "error.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <pthread.h>
#include <string>
#include <thread>

namespace hycast {

// Convenience types
using Thread       = std::thread;                ///< Type of a thread
using Mutex        = std::mutex;                 ///< Type of a mutex
using Guard        = std::lock_guard<Mutex>;     ///< Type of a guard lock
using Lock         = std::unique_lock<Mutex>;    ///< Type of a condition variable lock
using Cond         = std::condition_variable;    ///< Type of a condition variable
using String       = std::string;                ///< Type of a string
using SysClock     = std::chrono::system_clock;  ///< Type of the system clock
using SysTimePoint = SysClock::time_point;       ///< Type of a system time point
using SysDuration  = SysClock::duration;         ///< Type of a system clock duration

/// Ratio of the SysClock period to one second
constexpr double SysClockRatio = (static_cast<double>(SysDuration::period::num)) /
        SysDuration::period::den;

/**
 * RAII class for ensuring that a joinable thread halts and gets joined when an instance of this
 * class goes out of scope.
 */
class ThreadGuard
{
public:
    using HaltFunc = std::function<void()>; ///< Type of function used to halt the thread

private:
    Thread    thread; ///< The thread being guarded
    HaltFunc halt;   ///< The function to halt the thread

    /// Cancels the thread.
    void cancel() {
        const auto threadHandle = thread.native_handle();
        LOG_DEBUG("Cancelling thread " + std::to_string(threadHandle));
        const auto status = ::pthread_cancel(threadHandle);
        if (status)
            LOG_SYSERR("Couldn't cancel thread " + std::to_string(threadHandle) + ": " +
                    ::strerror(status));
    }

public:
    /**
     * Constructs from a thread and a function to halt the thread.
     * @param[in] thread  The thread. After construction, `thread` will not reference any thread.
     * @param[in] halt    The function to halt the thread
     */
    ThreadGuard(
            Thread&& thread,
            HaltFunc halt)
        : thread(std::move(thread))
        , halt(halt)
    {
        LOG_DEBUG("Guarding thread " + std::to_string(this->thread.native_handle()));
    }

    /**
     * Constructs from a thread. The thread will canceled on destruction.
     * @param[in] thread  The thread. After construction, `thread` will not reference any thread.
     */
    ThreadGuard(Thread&& thread)
        : ThreadGuard(std::move(thread), [&]{this->cancel();})
    {}

    /**
     * Destroys. If the thread is joinable, then the halt function will be called and the thread
     * will be joined.
     */
    ~ThreadGuard() {
        if (thread.joinable()) {
            const auto nativeHandle = thread.native_handle();
            LOG_DEBUG("Halting thread " + std::to_string(nativeHandle));
            halt();
            LOG_DEBUG("Joining thread " + std::to_string(nativeHandle));
            thread.join();
            LOG_DEBUG("Joined thread " + std::to_string(nativeHandle));
        }
    }

    ThreadGuard(const ThreadGuard&) =delete;
    ThreadGuard& operator=(const ThreadGuard& rhs) =delete;
};

} // namespace

namespace std {
    hycast::String to_string(const hycast::SysTimePoint& timePoint);
}

#endif /* MAIN_MISC_COMMONTYPES_H_ */
