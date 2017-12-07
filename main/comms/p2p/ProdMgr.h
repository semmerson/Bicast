/**
 * This file declares the interface for an object that manages product
 * reception.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerMgr.h
 * @author: Steven R. Emmerson
 */

#ifndef PRODMGR_H_
#define PRODMGR_H_

#include "Chunk.h"
#include "Peer.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include <memory>
#include "../RecvStatus.h"

namespace hycast {

class ProdMgr {
public:
    virtual ~ProdMgr() =default;
    /**
     * Starts sending the backlog of data-chunks to the remote peer.
     * @param[in] chunkId  Identifier of earliest data-chunk in backlog
     */
    void startBacklog(const ChunkId& chunkId, Peer& peer) =0;
    /**
     * Indicates whether or not available product-information should be
     * requested.
     * @param[in] prodIndex  Index of relevant product
     */
    virtual bool shouldBeRequested(const ProdIndex& prodIndex) =0;
    /**
     * Indicates whether or not an available chunk-of-data should be requested.
     * @param[in]     chunkId  Identifier of relevant data-chunk
     */
    virtual bool shouldBeRequested(const ChunkId& chunkId) =0;
    /**
     * Returns information about a product.
     * @param[in]  index     Index of relevant product
     * @param[out] prodInfo  Product information
     * @retval `true`        Information exists. `prodInfo` is set.
     * @retval `false`       Information doesn't exist. `prodInfo` is not set.
     */
    virtual bool get(const ProdIndex& prodIndex, ProdInfo& prodInfo) =0;
    /**
     * Returns a chunk-of-data.
     * @param[in]  chunkId  Identifier of relevant chunk
     * @param[out] chunk    Data-chunk
     * @retval `true`       Chunk exists. `chunk` is set.
     * @retval `false`      Chunk doesn't exist. `chunk` is not set.
     */
    virtual bool get(const ChunkId& chunkId, ActualChunk& chunk) =0;
    /**
     * Processes information about a product.
     * @param[in] info      Product information
     * @param[in] peerAddr  Address of remote peer
     */
    virtual void process(const ProdInfo& info, const InetSockAddr& peerAddr) =0;
    /**
     * Processes a chunk-of-data.
     * @param[in] chunk     Data-chunk
     * @param[in] peerAddr  Address of remote peer
     */
    virtual void process(LatentChunk& chunk, const InetSockAddr& peerAddr) =0;
};

} // namespace

#endif /* PRODMGR_H_ */
