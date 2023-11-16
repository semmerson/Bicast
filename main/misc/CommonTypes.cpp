/**
 * This file defines some common types.
 *
 *  @file:  CommonTypes.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include "CommonTypes.h"

namespace std {

using namespace bicast;

/// Returns the string representation of a system time-point
string to_string(const SysTimePoint& timePoint) {
    time_t      secs = SysClock::to_time_t(timePoint);
    struct tm   tmStruct;
    ::gmtime_r(&secs, &tmStruct);
    char        iso8601[28]; // "YYYY-MM-DDThh:mm:ss.uuuuuuZ"
    auto        nbytes = ::strftime(iso8601, sizeof(iso8601), "%FT%T", &tmStruct);
    long        usecs = chrono::duration_cast<chrono::microseconds>(
            timePoint - SysClock::from_time_t(secs)).count();
    if (usecs < 0) {
        --secs;
        usecs += 1000000;
    }
    ::snprintf(iso8601+nbytes, sizeof(iso8601)-nbytes, ".%06ldZ", usecs);
    return iso8601;
}

/// Returns the string representation of a system time-duration
string to_string(const SysDuration& duration) {
    return std::to_string(duration.count()*sysClockRatio) + " s";
}

/// Encodes a system time-point to an output stream.
ostream& operator<<(ostream& ostream, const SysTimePoint& time) {
    return ostream << to_string(time);
}

/// Encodes a system duration to an output stream.
ostream& operator<<(ostream& ostream, const SysDuration& dur) {
    return ostream << to_string(dur);
}

} // namespace
