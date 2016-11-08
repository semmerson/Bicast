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
#include "PeerMgr.h"
#include "ProdInfo.h"
#include "Socket.h"

#include <cstddef>
#include <memory>

namespace hycast {

class PeerImpl; // Forward declaration

class Peer final {
    std::shared_ptr<PeerImpl> pImpl;
public:
    /**
     * Constructs from nothing. Any attempt to use the resulting instance will
     * throw an exception.
     */
    Peer() = default;
    /**
     * Constructs from a peer manager and a socket. Doesn't receive anything
     * until `runReceiver()` is called.
     * @param[in,out] peerMgr  Peer manager. Must exist for the duration of the
     *                         constructed instance.
     * @param[in,out] sock     Socket
     * @param[in]     version  Protocol version
     * @see runReceiver()
     */
    Peer(
            PeerMgr& peerMgr,
            Socket&  sock);
    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance
     * @return `true` iff this instance is equal to the other
     */
    bool operator==(const Peer& that) const noexcept;
    /**
     * Indicates if two peers are equal.
     * @param[in] peer1  First peer
     * @param[in] peer2  Second peer
     * @return `true` iff the two peers are equal
     * @execptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    static bool areEqual(
            const Peer& peer1,
            const Peer& peer2);
    /**
     * Returns the hash code of an instance.
     * @param[in] peer  The instance
     * @return The instance's hash code
     * @execptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    static size_t hash(const Peer& peer);
    /**
     * Runs the receiver. Objects are received from the socket and passed to the
     * appropriate peer-manager methods. Doesn't return until either the socket
     * is closed or an exception is thrown.
     * @throws
     * @exceptionsafety Basic guarantee
     * @threadsafefy    Thread-compatible but not thread-safe
     */
    void runReceiver();
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
     * Returns the number of streams.
     */
    static unsigned getNumStreams();
};

} // namespace

#endif /* PEER_H_ */
