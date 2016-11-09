/**
 * This file declares the implementation of a set of peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSetImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERSETIMPL_H_
#define PEERSETIMPL_H_

#include "ChunkInfo.h"
#include "Peer.h"
#include "ProdInfo.h"

#include <set>

namespace hycast {

class PeerSetImpl final {
    std::set<Peer> set;

public:
    /**
     * Constructs from nothing. The set will be empty.
     */
    PeerSetImpl()
        : set{} {}
    /**
     * Inserts a peer.
     * @param[in] peer  Peer to be inserted
     * @exceptionsafety Strong guarantee
     * @threadsafety    Compatible but not safe
     */
    void insert(Peer& peer) {
        set.insert(peer);
    }
    /**
     * Sends information about a product to the remote peers.
     * @param[in] prodInfo  Product information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo);
    /**
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] chunkInfo  Chunk information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo);
};

} // namespace

#endif /* PEERSETIMPL_H_ */
