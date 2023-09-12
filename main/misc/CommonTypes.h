/**
 * StdUsing.h
 *
 *  Created on: Aug 10, 2022
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_COMMONTYPES_H_
#define MAIN_MISC_COMMONTYPES_H_

#include <chrono>
#include <condition_variable>
#include <stdint.h>
#include <mutex>
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

} // namespace

namespace std {
    using namespace hycast;

    String to_string(const SysTimePoint& timePoint);
}

#endif /* MAIN_MISC_COMMONTYPES_H_ */
