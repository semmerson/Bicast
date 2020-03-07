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

class PeerSet
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    class Observer
    {
    public:
        virtual ~Observer() =0;

        virtual void stopped(Peer peer) =0;
    };

    /**
     * Default constructs.
     */
    PeerSet();

    /**
     * Constructs.
     *
     * @param[in] observer  Observer to be notified if and when a peer stops
     *                      due to throwing an exception
     */
    PeerSet(Observer& obs);

    /**
     * Adds a peer to the set.
     *
     * @param[in] peer     The peer to add. `peer()` should not have been
     *                     called.
     * @retval    `true`   Success
     * @retval    `false`  The peer is already in the set
     * @threadsafety       Safe
     */
    bool activate(const Peer peer);

    /**
     * Notifies all the peers in the set of available product-information.
     *
     * @param[in] prodIndex  Indentifier of product
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

    /**
     * Returns the number of active peers in the set.
     *
     * @return        Number of active peers
     * @threadsafety  Safe
     */
    size_t size() const noexcept;
};

} // namespace

#endif /* MAIN_PEER_PEERSET_H_ */
