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

#include "Notifier.h"
#include "Peer.h"

#include <memory>

namespace hycast {

class PeerSetImpl; // Forward declaration

class PeerSet final : public Notifier
{
    std::shared_ptr<PeerSetImpl> pImpl; // `pImpl` idiom
public:
    typedef enum {
        FAILURE,
        SUCCESS,
        REPLACED
    } InsertStatus;
    /**
     * Constructs from the maximum number of peers. The set will be empty.
     * @param[in] maxPeers        Maximum number of peers
     * @param[in] stasisDuration  Required duration, in seconds, without change
     *                            to the set of peers before the
     *                            worst-performing peer may be replaced
     */
    explicit PeerSet(
            unsigned maxPeers = 8,
            unsigned stasisDuration = 60);
    /**
     * Tries to insert a peer.
     * @param[in]  candidate Candidate peer
     * @param[out] worst     Replaced, worst-performing peer
     * @return               Status of the attempted insertion:
     *   - PeerSet::FAILURE  Insertion was rejected
     *   - PeerSet::SUCCESS  Insertion was successful
     *   - PeerSet::REPLACED Insertion was successful and `*worst` is set if
     *     `worst != nullptr`
     * @exceptionsafety      Strong guarantee
     * @threadsafety         Safe
     */
    InsertStatus tryInsert(
            Peer& candidate,
            Peer* worst = nullptr) const;
    /**
     * Sends information about a product.
     * @param[in] prodInfo        Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo) const;
    /**
     * Sends information about a chunk-of-data.
     * @param[in] chunkInfo       Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
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
