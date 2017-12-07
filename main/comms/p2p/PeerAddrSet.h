/**
 * This file declares a thread-safe entry for an available chunk-of-data that's
 * been requested but not yet received.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PendingChunkEntry.h
 *  Created on: Dec 7, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_P2P_PEERADDRSET_H_
#define MAIN_COMMS_P2P_PEERADDRSET_H_

#include "InetSockAddr.h"

#include <memory>
#include <random>

namespace hycast {

class PeerAddrSet final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     */
    PeerAddrSet();

    /**
     * Returns the number of peer-addresses.
     * @return Number of peer-addresses
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    size_t size() const;

    /**
     * Adds a peer-address.
     * @param[in] peerAddr  Peer-address to be added
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    void add(const InetSockAddr& peerAddr);

    /**
     * Removes a peer-address.
     * @param[in] peerAddr  Peer-address to be removed
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    void remove(const InetSockAddr& peerAddr);

    /**
     * Returns a peer-address chosen at random.
     * @param[out] peerAddr  Pseudo-randomly-chosen peer-address. Set if the set
     *                       isn't empty.
     * @generator            Pseudo-random number generator
     * @retval     `true`    Iff `peerAddr` is set
     * @exceptionsafety      Strong guarantee
     * @threadsafety         Safe
     */
    bool getRandom(
            InetSockAddr&               peerAddr,
            std::default_random_engine& generator) const;
};

} // namespace

#endif /* MAIN_COMMS_P2P_PEERADDRSET_H_ */
