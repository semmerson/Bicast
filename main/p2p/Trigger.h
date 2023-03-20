/**
 * This file declares a trigger for when the worst-performing peer should be
 * replaced.
 *
 *  @file:  Trigger.h
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

#ifndef MAIN_P2P_TRIGGER_H_
#define MAIN_P2P_TRIGGER_H_

#include <chrono>
#include <memory>

namespace hycast {

/// Interface for trigger indicating when to replace worst-performing peer
class Trigger
{
public:
    using Duration = std::chrono::milliseconds; ///< Type of duration
    /// Smart pointer to the implementation
    using Pimpl    = std::shared_ptr<Trigger>;

    virtual ~Trigger() {};

    /**
     * Returns a trigger based on elapsed time.
     *
     * @param[in] duration  Amount of time to wait before returning
     */
    static Trigger::Pimpl create(const Duration& duration);

    /**
     * Returns a trigger based on data reception.
     *
     * @param[in] numBytes  Number of bytes to receive before returning
     * @see `received()`
     */
    static Trigger::Pimpl create(const size_t numBytes);

    /**
     * Processes reception of data (for reception-based triggers).
     *
     * @param[in] numBytes  Amount of data received in bytes
     */
    virtual void received(const size_t numBytes) =0;

    /**
     * Returns when the worst-performing peer should be replaced. Blocks until then.
     *
     * @see `reset()`
     */
    virtual void sayWhen() =0;

    /**
     * Resets. Should be called before `sayWhen()`.
     *
     * @see `sayWhen()`
     */
    virtual void reset() =0;
};

} // namespace

#endif /* MAIN_P2P_TRIGGER_H_ */
