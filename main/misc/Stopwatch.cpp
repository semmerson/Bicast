/**
 * This file implements a template for a simple stopwatch.
 *
 *  @file:  Stopwatch.cpp
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

#include "config.h"

#include "Stopwatch.h"

namespace bicast {

/// Implementation of a stopwatch
template<class DUR>
class Stopwatch<DUR>::Impl {
    using Clock = std::chrono::steady_clock;

    Clock::time_point begin;

public:
    Impl()
        : begin()
    {}

    void start() { ///< Starts this instance
        begin = Clock::now();
    }

    DUR split() { ///< Returns the split time
        return std::chrono::duration_cast<DUR>(Clock::now() - begin);
    }
};

template<class DUR>
Stopwatch<DUR>::Stopwatch()
    : pImpl(new Impl())
{}

template<class DUR>
void Stopwatch<DUR>::start() {
    pImpl->start();
}

template<class DUR>
DUR Stopwatch<DUR>::split() {
    return pImpl->split();
}

} // namespace
