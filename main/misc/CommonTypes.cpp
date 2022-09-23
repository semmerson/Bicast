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

String to_string(const SysTimePoint& timePoint) {
    auto        secs = SysClock::to_time_t(timePoint);
    struct tm   tmStruct;
    ::gmtime_r(&secs, &tmStruct);
    char        iso8601[28]; // "YYYY-MM-DDThh:mm:ss.uuuuuuZ"
    auto        nbytes = ::strftime(iso8601, sizeof(iso8601), "%FT%T", &tmStruct);
    long        usecs = std::chrono::duration_cast<std::chrono::microseconds>(
            timePoint - SysClock::from_time_t(secs)).count();
    ::snprintf(iso8601+nbytes, sizeof(iso8601)-nbytes, ".%06ldZ", usecs);
    return iso8601;
}

} // namespace