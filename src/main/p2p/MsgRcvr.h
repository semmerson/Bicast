/**
 * This file declares the interface for an object that receives messages from a
 * remote peer.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: MsgRcvr.h
 * @author: Steven R. Emmerson
 */

#ifndef MSGRCVR_H_
#define MSGRCVR_H_

#include "Chunk.h"
#include "ChunkInfo.h"
#include "Peer.h"
#include "ProdIndex.h"
#include "ProdInfo.h"

#include <memory>

namespace hycast {

class MsgRcvrImpl; // Forward declaration

class MsgRcvr final {
    std::shared_ptr<MsgRcvrImpl> pImpl; // `pImpl` idiom
public:
    /**
     * Constructs from nothing.
     */
    MsgRcvr() =default;
    /**
     * Constructs from an implementation.
     * @param[in,out] impl  The implementation
     */
    MsgRcvr(MsgRcvrImpl* impl);
    /**
     * Receives a notice about a new product.
     * @param[in]     info  Information about the product
     * @param[in,out] peer  Peer that received the notice
     */
    void recvNotice(const ProdInfo& info, Peer& peer) const;
    /**
     * Receives a notice about a chunk-of-data.
     * @param[in]     info  Information about the chunk
     * @param[in,out] peer  Peer that received the notice
     */
    void recvNotice(const ChunkInfo& info, Peer& peer) const;
    /**
     * Receives a request for information about a product.
     * @param[in]     index Index of the product
     * @param[in,out] peer  Peer that received the request
     */
    void recvRequest(const ProdIndex& index, Peer& peer) const;
    /**
     * Receives a request for a chunk-of-data.
     * @param[in]     info  Information on the chunk
     * @param[in,out] peer  Peer that received the request
     */
    void recvRequest(const ChunkInfo& info, Peer& peer) const;
    /**
     * Receives a chunk-of-data.
     * @param[in]     chunk  Chunk-of-data
     * @param[in,out] peer   Peer that received the chunk
     */
    /*
     * For an unknown reason, the compiler complains if the `recvData` parameter
     * is a `LatentChunk&` and not a `LatentChunk`. This is acceptable, however,
     * because `LatentChunk` uses the pImpl idiom. See
     * `PeerConnectionImpl::runReceiver`.
     */
    void recvData(LatentChunk chunk, Peer& peer) const;
};

} // namespace

#endif /* MSGRCVR_H_ */
