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
using Thread       = std::thread;
using Mutex        = std::mutex;
using Guard        = std::lock_guard<Mutex>;
using Lock         = std::unique_lock<Mutex>;
using Cond         = std::condition_variable;
using String       = std::string;
using SysClock     = std::chrono::system_clock;
using SysTimePoint = SysClock::time_point;
using SysDuration  = SysClock::duration;

} // namespace

namespace std {
    using namespace hycast;

    String to_string(const SysTimePoint& timePoint);
}

#endif /* MAIN_MISC_COMMONTYPES_H_ */
