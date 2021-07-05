/**
 * Thread-safe, dynamic set of active peers.
 *
 *        File: PeerSet.h
 *  Created on: Jun 7, 2019
 *      Author: Steven R. Emmerson
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

#ifndef MAIN_PEER_PEERSET_H_
#define MAIN_PEER_PEERSET_H_

#include "Peer.h"

#include <memory>

namespace hycast {

/**
 * Interface for a manager of a set of active peers.
 */
class PeerSetMgr
{
public:
    /**
     * Destroys.
     */
    virtual ~PeerSetMgr() noexcept =default;

    /**
     * Handles the stopping of a peer.
     *
     * @param[in] peer  Peer that stopped
     */
    virtual void stopped(Peer peer) =0;
};


/**
 * A set of active peers.
 */
class PeerSet final
{
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    PeerSet() =default;

    /**
     * Constructs.
     *
     * @param[in] peerSetMgr  Manager of this instance to be notified if and
     *                        when a peer stops due to throwing an exception.
     *                        Must exist for the duration of this instance.
     */
    PeerSet(PeerSetMgr& peerSetMgr);

    /**
     * Adds a peer to the set of active peers.
     *
     * @param[in] peer        Peer to be activated
     * @threadsafety          Safe
     * @exceptionSafety       Strong guarantee
     * @cancellationpoint     No
     */
    void activate(const Peer peer);

    /**
     * Synchronously halts all peers in the set. Doesn't return until the set is
     * empty.
     */
    void halt();

    /**
     * Returns the number of active peers in the set.
     *
     * @return        Number of active peers
     * @threadsafety  Safe
     */
    size_t size() const noexcept;

    /**
     * Notifies all peers in the set, except one, that the local node has
     * transitioned from not having a path to the source of data-products to
     * having one.
     *
     * @param[in] notPeer  Peer to skip
     */
    void gotPath(Peer notPeer);

    /**
     * Notifies all peers in the set, except one, that the local node has
     * transitioned from having a path to the source of data-products to
     * not having one.
     *
     * @param[in] notPeer  Peer to skip
     */
    void lostPath(Peer notPeer);

    /**
     * Notifies all the peers in the set of available product-information.
     *
     * @param[in] prodIndex  Identifier of product
     */
    void notify(ProdIndex prodIndex);

    /**
     * Notifies all the peers in the set -- except one -- of available
     * product-information.
     *
     * @param[in] prodIndex  Identifier of product
     * @param[in] notPeer    Peer not to be notified
     */
    void notify(
            const ProdIndex prodIndex,
            const Peer&     notPeer);

    /**
     * Notifies all the peers in the set of an available data-segment.
     *
     * @param[in] segId  Identifier of data-segment
     */
    void notify(const SegId& segId);

    /**
     * Notifies all the peers in the set -- except one -- of an available
     * data-segment.
     *
     * @param[in] segId    Identifier of data-segment
     * @param[in] notPeer  Peer not to be notified
     */
    void notify(
            const SegId& segId,
            const Peer&  notPeer);
};

} // namespace

#endif /* MAIN_PEER_PEERSET_H_ */
