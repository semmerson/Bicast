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

#include "Chunk.h"
#include "ChunkInfo.h"
#include "ProdInfo.h"
#include "Socket.h"

#include <memory>

namespace hycast {

class PeerImpl; // Forward declaration

typedef enum {
    MSGTYPE_EOF = 0,
    MSGTYPE_PROD_NOTICE,
    MSGTYPE_CHUNK_NOTICE,
    MSGTYPE_PROD_REQUEST,
    MSGTYPE_CHUNK_REQUEST,
    MSGTYPE_CHUNK,
    MSGTYPE_NUM_TYPES
} MsgType;

class Peer final {
    std::shared_ptr<PeerImpl> pImpl;
public:
    /**
     * Constructs from an SCTP socket.
     * @param[in,out] sock     Socket
     */
    Peer(Socket& sock);
    /**
     * Returns the number of SCTP streams.
     */
    static unsigned getNumStreams();
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
    /**
     * Returns the type of the next message. Blocks until one arrives if
     * necessary.
     */
    MsgType getMsgType();
    /**
     * Returns a notice of a product.
     * @pre `getMsgType() == MSGTYPE_PROD_NOTICE`
     * @return Product notice
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
    ProdInfo getProdNotice();
    /**
     * Returns a notice of a chunk-of-data.
     * @pre `getMsgType() == MSGTYPE_CHUNK_NOTICE`
     * @return Chunk-of-data
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
    ChunkInfo getChunkNotice();
    /**
     * Returns a request for information on a product.
     * @pre `getMsgType() == MSGTYPE_PROD_REQUEST`
     * @return Product index
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
    ProdIndex getProdRequest();
    /**
     * Returns a request for a chunk-of-data.
     * @pre `getMsgType() == MSGTYPE_CHUNK_REQUEST`
     * @return Information on the chunk
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
    ChunkInfo getChunkRequest();
    /**
     * Returns a latent chunk-of-data. All the `get*()` methods will block
     * until method `LatentChunk.drainData(void*)` is called on the returned
     * object by the current thread. Undefined behavior results if that method
     * is called by any other thread.
     * @pre `getMsgType() == MSGTYPE_CHUNK`
     * @return Latent chunk-of-data
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
    LatentChunk getChunk();
};

} // namespace

#endif /* PEER_H_ */
