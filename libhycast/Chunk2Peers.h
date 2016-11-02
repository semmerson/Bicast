/**
 * This file declares a mapping from a chunk-of-data to the peers that have it.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Chunk2Peers.h
 * @author: Steven R. Emmerson
 */

#ifndef CHUNK2PEERS_H_
#define CHUNK2PEERS_H_

#include "ChunkInfo.h"
#include "Peer.h"

#include <list>
#include <unordered_map>

namespace hycast {

class Chunk2Peers final {
    std::unordered_map<class ChunkInfo, std::list<class Peer>,
            decltype(&ChunkInfo::hash), decltype(&ChunkInfo::areEqual)> map;
public:
    Chunk2Peers()
        : map{16, &ChunkInfo::hash, &ChunkInfo::areEqual}
    {}
    /**
     * Adds a peer to the list of peers that have a particular chunk-of-data.
     * The list is ordered by insertion with the first added peer at the front.
     * @param[in] info  Information on the data-chunk
     * @param[in] peer  The peer that has the data-chunk
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void add(
            const ChunkInfo& info,
            Peer&            peer);
    /**
     * Returns the peer at the front of the list of peers that have a particular
     * data-chunk.
     * @param[in] info  Information on the data-chunk
     * @return The peer at the front of the list of peers that have the
     *         data-chunk or `nullptr` if no such peer exists
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    Peer* getFrontPeer(const ChunkInfo& info);
    /**
     * Removes a particular data-chunk and all the peers that have it.
     * @param[in] info  Information on the data-chunk
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void remove(const ChunkInfo& info);
    /**
     * Removes a peer from from the list of peers that have a particular
     * data-chunk.
     * @param[in] chunk  The data-chunk
     * @param[in] peer   The peer to be removed from the list of peers that have
     *                   the data chunk
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void remove(
            const ChunkInfo& info,
            const Peer&      peer);
};

} // namespace

#endif /* CHUNK2PEERS_H_ */
