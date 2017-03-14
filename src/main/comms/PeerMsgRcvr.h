/**
 * This file declares the interface for an object that receives unicast messages
 * from one or more remote peers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerMsgRcvr.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERMSGRCVR_H_
#define PEERMSGRCVR_H_

#include "Chunk.h"
#include "ChunkInfo.h"
#include "Peer.h"
#include "ProdIndex.h"
#include "ProdInfo.h"

#include <memory>

namespace hycast {

class PeerMsgRcvr {
public:
    virtual ~PeerMsgRcvr() =default;
    /**
     * Receives a notice about a new product.
     * @param[in]     info  Information about the product
     * @param[in,out] peer  Peer that received the notice
     */
    virtual void recvNotice(const ProdInfo& info, Peer& peer) =0;
    /**
     * Receives a notice about a chunk-of-data.
     * @param[in]     info  Information about the chunk
     * @param[in,out] peer  Peer that received the notice
     */
    virtual void recvNotice(const ChunkInfo& info, Peer& peer) =0;
    /**
     * Receives a request for information about a product.
     * @param[in]     index Index of the product
     * @param[in,out] peer  Peer that received the request
     */
    virtual void recvRequest(const ProdIndex& index, Peer& peer) =0;
    /**
     * Receives a request for a chunk-of-data.
     * @param[in]     info  Information on the chunk
     * @param[in,out] peer  Peer that received the request
     */
    virtual void recvRequest(const ChunkInfo& info, Peer& peer) =0;
    /**
     * Receives a chunk-of-data.
     * @param[in]     chunk  Chunk-of-data
     * @param[in,out] peer   Peer that received the chunk
     */
    /*
     * For an unknown reason, the compiler complains if the `recvData` parameter
     * is a `LatentChunk&` and not a `LatentChunk`. This is acceptable, however,
     * because `LatentChunk` uses the pImpl idiom. See
     * `PeerImpl::runReceiver()`.
     */
    virtual void recvData(LatentChunk chunk, Peer& peer) =0;
};

} // namespace

#endif /* PEERMSGRCVR_H_ */
