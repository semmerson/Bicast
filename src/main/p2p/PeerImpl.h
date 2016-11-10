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

#ifndef PEERIMPL_H_
#define PEERIMPL_H_

#include "ChunkChannel.h"
#include "ChunkInfo.h"
#include "VersionMsg.h"
#include "PeerMgr.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include "RegChannel.h"
#include "Socket.h"

#include <cstddef>
#include <functional>
#include <thread>

namespace hycast {

class PeerImpl final {
    typedef enum {
        VERSION_STREAM_ID = 0,
        PROD_NOTICE_STREAM_ID,
        CHUNK_NOTICE_STREAM_ID,
        PROD_REQ_STREAM_ID,
        CHUNK_REQ_STREAM_ID,
        CHUNK_STREAM_ID,
        NUM_STREAM_IDS
    }      SctpStreamId;
    unsigned               version;
    RegChannel<VersionMsg> versionChan;
    RegChannel<ProdInfo>   prodNoticeChan;
    RegChannel<ChunkInfo>  chunkNoticeChan;
    RegChannel<ProdIndex>  prodReqChan;
    RegChannel<ChunkInfo>  chunkReqChan;
    ChunkChannel           chunkChan;
    PeerMgr*               peerMgr;
    Socket                 sock;
    Peer*                  peer;

    /**
     * Returns the protocol version of the remote peer.
     * @pre `sock.getStreamId() == VERSION_STREAM_ID`
     * @return Protocol version of the remote peer
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
     unsigned getVersion();
     /**
      * Every peer implementation is unique.
      */
     PeerImpl(const PeerImpl& impl) =delete;
     PeerImpl(const PeerImpl&& impl) =delete;
     PeerImpl& operator=(const PeerImpl& impl) =delete;
     PeerImpl& operator=(const PeerImpl&& impl) =delete;

public:
    /**
     * Constructs from nothing. Any attempt to use use resulting instance will
     * throw an exception.
     */
    PeerImpl();
    /**
     * Constructs from a peer, a socket, and a protocol version. Immediately
     * starts receiving objects from the socket and passing them to the
     * appropriate peer methods.
     * @param[in,out] peerMgr  Peer manager. Must exist for the duration of the
     *                         constructed instance.
     * @param[in,out] sock     Socket
     */
    PeerImpl(
            PeerMgr& peerMgr,
            Socket&  sock,
            Peer&    peer);
    /**
     * Returns the number of streams.
     */
    static unsigned getNumStreams();
    /**
     * Runs the receiver. Objects are received from the socket and passed to the
     * appropriate peer manager methods. Doesn't return until either the socket
     * is closed or an exception is thrown.
     * @throws
     * @exceptionsafety Basic guarantee
     * @threadsafefy    Thread-compatible but not thread-safe
     */
    void runReceiver();
    /**
     * Sends information about a product to the remote peer.
     * @param[in] prodInfo  Product information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendProdInfo(const ProdInfo& prodInfo) const;
    /**
     * Sends information about a chunk-of-data to the remote peer.
     * @param[in] chunkInfo  Chunk information
     */
    void sendChunkInfo(const ChunkInfo& chunkInfo);
    /**
     * Sends a request for product information to the remote peer.
     * @param[in] prodIndex  Product-index
     */
    void sendProdRequest(const ProdIndex& prodIndex);
    /**
     * Sends a request for a chunk-of-data to the remote peer.
     * @param[in] info  Chunk specification
     */
    void sendRequest(const ChunkInfo& info);
    /**
     * Sends a chunk-of-data to the remote peer.
     * @param[in] chunk  Chunk-of-data
     */
    void sendData(const ActualChunk& chunk);
    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @return `true` iff this instance equals the other
     * @execptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    bool equals(const PeerImpl& that) const {
        return this == &that; // Every instance is unique
    }
    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @return `true` iff this instance equals the other
     * @execptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    bool operator==(const PeerImpl& that) const {
        return this == &that; // Every instance is unique
    }
    /**
     * Indicates if this instance is less than another.
     * @param that  Other instance
     * @return `true` iff this instance is less that the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator<(const PeerImpl& that) const noexcept {
        return this < &that;
    }
    /**
     * Returns the hash code of this instance.
     * @return the hash code of this instance
     * @execptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    size_t hash() const noexcept {
        return std::hash<const PeerImpl*>()(this); // Every instance is unique
    }
};

} // namespace

#endif /* PEERIMPL_H_ */
