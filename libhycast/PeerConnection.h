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

#ifndef PEERCONNECTION_H_
#define PEERCONNECTION_H_

#include "ChunkInfo.h"
#include "Peer.h"
#include "ProdInfo.h"
#include "Socket.h"

#include <memory>

namespace hycast {

class PeerConnectionImpl; // Forward declaration

class PeerConnection final {
    std::shared_ptr<PeerConnectionImpl> pImpl;
public:
    /**
     * Constructs from a peer, a socket, and a protocol version. Immediately
     * starts receiving objects from the socket and passing them to the
     * appropriate peer methods.
     * @param[in,out] peer     Peer. Must exist for the duration of the
     *                         constructed instance.
     * @param[in,out] sock     Socket
     * @param[in]     version  Protocol version
     */
    PeerConnection(
            Peer&          peer,
            Socket&        sock,
            const unsigned version);
    /**
     * Sends information about a product to the remote peer.
     * @param[in] prodInfo  Product information
     */
    void sendProdInfo(const ProdInfo& prodInfo);
    /**
     * Sends information about a chunk-of-data to the remote peer.
     * @param[in] chunkInfo  Chunk information
     */
    void sendChunkInfo(const ChunkInfo& chunkInfo);
    /**
     * Sends a product-index to the remote peer.
     * @param[in] prodIndex  Product-index
     */
    void sendProdRequest(const ProdIndex& prodIndex);
    /**
     * Sends a chunk specification to the remote peer.
     * @param[in] prodIndex  Product-index
     */
    void sendRequest(const ChunkInfo& info);
};

} // namespace

#endif /* PEERCONNECTION_H_ */
