/**
 * This file declares interfaces for the Bicast multicast component.
 *
 *  @file:  mcast.h
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

#ifndef MAIN_MCAST_MCAST_H_
#define MAIN_MCAST_MCAST_H_

#include "BicastProto.h"
#include "SockAddr.h"

#include <memory>

namespace bicast {

class SubNode;
using SubNodePtr = std::shared_ptr<SubNode>;

/// Interface for a multicast publisher
class McastPub
{
public:
    /// Smart pointer to the implementation
    using Pimpl = std::shared_ptr<McastPub>;

    /**
     * Returns a new instance.
     *
     * @return                 New instance
     * @throw InvalidArgument  Multicast group IP address isn't source-specific
     */
    static Pimpl create();

    virtual ~McastPub() noexcept {}

    /**
     * Multicasts information on a product.
     *
     * @param[in] prodInfo  Product information to be multicast
     * @throw SYSTEM_ERROR  Multicast transport closed
     */
    virtual void multicast(const ProdInfo prodInfo) =0;

    /**
     * Multicasts a data-segment.
     *
     * @param[in] dataSeg   Data-segment to be multicast
     * @throw SYSTEM_ERROR  Multicast transport closed
     */
    virtual void multicast(const DataSeg dataSeg) =0;
};

/// Interface for a multicast subscriber
class McastSub
{
public:
    /// Smart pointer to the implementation
    using Pimpl = std::shared_ptr<McastSub>;

    /**
     * Returns a new instance.
     *
     * @param[in] mcastAddr        Socket address of multicast group
     * @param[in] srcAddr          Internet address of publisher
     * @param[in] node             Subscribing node to call
     * @return                     New instance
     * @throw     InvalidArgument  Multicast group IP address isn't source-specific
     * @throw     LogicError       IP address families don't match
     */
    static Pimpl create(
            const SockAddr& mcastAddr,
            const InetAddr& srcAddr,
            SubNode*        node);

    /**
     * Idempotent. Calls `halt()`.
     *
     * @see `halt()`
     */
   virtual ~McastSub() noexcept {};

    /**
     * Executes this instance. Starts reading the multicast and calling the subscribing node.
     * Doesn't return until `halt()` is called or an exception is thrown.
     *
     * @throw SystemError  Multicast I/O failure
     * @see `halt()`
     */
    virtual void run() =0;

    /**
     * Halts operation at the next attempt to read the multicast. Causes `run()` to return.
     *
     * @see `run()`
     * @see `~McastSub()`
     */
    virtual void halt() =0;
};

} // namespace

#endif /* MAIN_MCAST_MCAST_H_ */
