/**
 * Thread-safe, dynamic set of active peers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerSet.h
 *  Created on: Jun 7, 2019
 *      Author: Steven R. Emmerson
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
     * @param[in] observer  Observer to be notified if and when a peer stops
     *                      due to throwing an exception. Must exist for the
     *                      duration of this instance.
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
