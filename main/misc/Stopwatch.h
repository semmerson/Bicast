/**
 * This file declares a template for a simple stopwatch.
 *
 *  @file:  Stopwatch.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
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

#ifndef MAIN_MISC_STOPWATCH_H_
#define MAIN_MISC_STOPWATCH_H_

#include <chrono>
#include <memory>

namespace bicast {

/// A stopwatch class
template<class DUR=std::chrono::steady_clock::duration>
class Stopwatch
{
    using Clock = std::chrono::steady_clock;

    Clock::time_point begin;

public:
    Stopwatch()
        : begin()
    {}

    void start() { ///< Starts this instance
        begin = Clock::now();
    }

    DUR split() { ///< Returns the split time
        return std::chrono::duration_cast<DUR>(Clock::now() - begin);
    }
};

} // namespace

#endif /* MAIN_MISC_STOPWATCH_H_ */
