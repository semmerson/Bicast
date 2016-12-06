/**
 * This file declares a set of peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSet.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERSET_H_
#define PEERSET_H_

#include "Peer.h"

#include <memory>

namespace hycast {

class PeerSetImpl; // Forward declaration

class PeerSet final {
    std::shared_ptr<PeerSetImpl> pImpl; // `pImpl` idiom
public:
    typedef enum {
        FAILURE,
        SUCCESS,
        REPLACED
    } InsertStatus;
    /**
     * Constructs from the maximum number of peers. The set will be empty.
     * @param[in] maxPeers     Maximum number of peers
     * @param[in] minDuration  Required duration before the worst-performing
     *                         peer may be replaced
     */
    PeerSet(
            unsigned              maxPeers = 8,
            std::chrono::seconds  minDuration = std::chrono::seconds{60});
    /**
     * Tries to insert a peer.
     * @param[in]  candidate Candidate peer
     * @param[out] worst     Replaced, worst-performing peer
     * @return The status of the attempted insertion. `*worst` is set if the
     *         returned status is `REPLACED` and `worst != nullptr`.
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    InsertStatus tryInsert(
            Peer& candidate,
            Peer* worst = nullptr) const;
    /**
     * Sends information about a product to the remote peers.
     * @param[in] prodInfo  Product information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo) const;
    /**
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] chunkInfo  Chunk information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo) const;
    /**
     * Increments the value of a peer.
     * @param[in] peer  Peer to have its value incremented
     */
    void incValue(Peer& peer) const;
};

} // namespace

#endif /* PEERSET_H_ */
