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
    std::shared_ptr<PeerSetImpl> pImpl;
public:
    /**
     * Constructs from nothing. The set will be empty.
     */
    PeerSet();
    /**
     * Inserts a peer.
     * @param[in] peer  Peer to be inserted
     * @exceptionsafety Strong guarantee
     * @threadsafety    Compatible but not safe
     */
    void insert(Peer& peer) const;
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
};

} // namespace

#endif /* PEERSET_H_ */
