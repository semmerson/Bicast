/**
 * This file declares a connection between peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection.h
 * @author: Steven R. Emmerson
 */

#ifndef PEER_H_
#define PEER_H_

#include "ChunkInfo.h"
#include "ProdInfo.h"
#include "Socket.h"

#include <memory>
#include "PeerMgr.h"

namespace hycast {

class PeerImpl; // Forward declaration

class Peer final {
    std::shared_ptr<PeerImpl> pImpl;
public:
    /**
     * Constructs from a peer manager, a socket, and a protocol version.
     * Immediately starts receiving objects from the socket and passing them to
     * the appropriate peer methods.
     * @param[in,out] peer     Peer. Must exist for the duration of the
     *                         constructed instance.
     * @param[in,out] sock     Socket
     * @param[in]     version  Protocol version
     */
    Peer(
            PeerMgr&       peer,
            Socket&        sock,
            const unsigned version);
    /**
     * Sends information about a product to the remote peer.
     * @param[in] prodInfo  Product information
     */
    void sendNotice(const ProdInfo& prodInfo);
    /**
     * Sends information about a chunk-of-data to the remote peer.
     * @param[in] chunkInfo  Chunk information
     */
    void sendNotice(const ChunkInfo& chunkInfo);
    /**
     * Sends a product-index to the remote peer.
     * @param[in] prodIndex  Product-index
     */
    void sendRequest(const ProdIndex& prodIndex);
    /**
     * Sends a chunk specification to the remote peer.
     * @param[in] prodIndex  Product-index
     */
    void sendRequest(const ChunkInfo& info);
    /**
     * Sends a chunk-of-data to the remote peer.
     * @param[in] chunk  Chunk-of-data
     */
    void sendData(const ActualChunk& chunk);
};

} // namespace

#endif /* PEER_H_ */
