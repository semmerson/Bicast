/**
 * This file defines a trigger for when the worst-performing peer should be
 * replaced.
 *
 *  @file:  Trigger.cpp
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

#include "HycastProto.h"
#include "Trigger.h"

#include <thread>

namespace hycast {

using namespace std::chrono;

/// Class that triggers replacement of worst-performing peer periodically.
class TimeTrigger : public Trigger
{
private:
    const Duration dur;

public:
    /**
     * Constructs.
     * @param[in] duration  Amount of time to wait
     */
    TimeTrigger(const Duration& duration)
        : dur(duration)
    {}

    void received(const size_t numBytes) override {
    }

    void sayWhen() override {
        std::this_thread::sleep_for(dur);
    }

    void reset() override {
    }
};

Trigger::Pimpl Trigger::create(const Duration& duration) {
    return Pimpl(new TimeTrigger(duration));
}

/**************************************************************************************************/

/// Class that triggers replacement of worst-performing peer when received data exceeds a threshold.
class DataTrigger : public Trigger
{
    mutable Mutex mutex;
    mutable Cond  cond;
    const size_t  threshold;
    size_t        soFar;

public:
    /**
     * Constructs.
     * @param[in] threshold  Number of bytes to wait for
     */
    DataTrigger(const size_t threshold)
        : mutex()
        , cond()
        , threshold(threshold)
        , soFar(0)
    {}

    void received(const size_t numBytes) override {
        Guard guard{mutex};
        soFar += numBytes;
        if (soFar >= threshold)
            cond.notify_one();
    }

    void sayWhen() override {
        Lock lock{mutex};
        cond.wait(lock, [&]{return soFar >= threshold;});
    }

    void reset() override {
        Guard guard{mutex};
        soFar = 0;
    }
};

Trigger::Pimpl Trigger::create(const size_t numBytes) {
    return Pimpl(new DataTrigger(numBytes));
}

} // namespace
