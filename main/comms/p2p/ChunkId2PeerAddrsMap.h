/**
 * This file declares a thread-safe map from a data-chunk identifier to the set
 * of peers that have the chunk.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: ChunkId2PeerAddrMap.h
 *  Created on: Dec 7, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_P2P_CHUNKID2PEERADDRMAP_H_
#define MAIN_COMMS_P2P_CHUNKID2PEERADDRMAP_H_

#include "Chunk.h"
#include "PeerAddrSet.h"

#include <memory>

namespace hycast {

class ChunkId2PeerAddrsMap final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     */
    ChunkId2PeerAddrsMap();

    /**
     * Returns the number of entries.
     * @return           Number of peer-addresses
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    size_t size() const;

    /**
     * Adds a peer-address. Increases the size of the map by one if the given
     * chunk identifier has no associated peer-addresses.
     * @param[in] chunkId   Data-chunk identifier
     * @param[in] peerAddr  Peer-address to be added
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    void add(
            const ChunkId&      chunkId,
            const InetSockAddr& peerAddr);

    /**
     * Removes a peer-address. Decreases the size of the map by one if the
     * address is the only one associated with the chunk identifier.
     * @param[in] chunkId   Data-chunk identifier
     * @param[in] peerAddr  Peer-address to be removed
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    void remove(
            const ChunkId&      chunkId,
            const InetSockAddr& peerAddr);

    /**
     * Removes an entry (the chunk-identifier and the associated
     * peer-addresses). Decreases the size of the map by one if the entry
     * exists.
     * @param[in] chunkId   Data-chunk identifier
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    void remove(const ChunkId& chunkId);

    /**
     * Pseudo-randomly chooses a peer-address that's associated with a
     * data-chunk identifier and returns it.
     * @param[in]  chunkId   Data-chunk identifier
     * @param[out] peerAddr  Pseudo-randomly-chosen peer-address associated with
     *                       the chunk identifier. Set if one exists.
     * @generator            Pseudo-random number generator
     * @retval     `true`    Iff `peerAddr` is set
     * @exceptionsafety      Strong guarantee
     * @threadsafety         Safe
     */
    bool getRandom(
            const ChunkId&              chunkId,
            InetSockAddr&               peerAddr,
            std::default_random_engine& generator) const;
};

} // namespace

#endif /* MAIN_COMMS_P2P_CHUNKID2PEERADDRMAP_H_ */
