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
#include "Notifier.h"
#include "PeerServer.h"
#include "ProdInfo.h"
#include "SctpSock.h"

#include <cstddef>
#include <memory>
#include <set>

namespace hycast {

class Peer final : public Notifier
{
    class                 Impl;  // Forward declaration
    std::shared_ptr<Impl> pImpl; // `pImpl` idiom

public:
    /// Type of container returned by `getOutstandingChunks()`
    typedef std::set<ChunkId> ChunkIdSet;

    /**
     * Constructs from nothing. Any attempt to use the resulting instance will
     * throw an exception.
     */
    Peer();

    /**
     * Constructs a server-side instance. Blocks until connected and versions
     * exchanged. This is a cancellation point.
     * @param[in] sock          Socket (as from `accept()`)
     * @throw     LogicError    Unknown protocol version from remote peer
     * @throw     RuntimeError  Couldn't construct peer
     * @throw     SystemError   Connection failure
     */
    Peer(SctpSock&  sock);

    /**
     * Constructs a client-side instance. Blocks until connected and versions
     * exchanged.
     * @param[in] peerAddr      Socket address of remote peer-server
     * @throw     LogicError    Unknown protocol version from remote peer
     * @throw     RuntimeError  Couldn't construct peer
     * @throw     SystemError   Connection failure
     */
    Peer(const InetSockAddr& peerAddr);

    /**
     * Returns the Internet socket address of the remote peer.
     * @return Internet socket address of remote peer
     */
    InetSockAddr getRemoteAddr() const;

    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @execptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    size_t hash() const noexcept;

    /**
     * Indicates if this instance is less than another.
     * @param that  Other instance
     * @return `true` iff this instance is less that the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator<(const PeerMsgSndr& that) const noexcept;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance
     * @return `true` iff this instance is equal to the other
     */
    bool operator==(const PeerMsgSndr& that) const noexcept;

    /**
     * Indicates if this instance is not equal to another.
     * @param[in] that  The other instance
     * @return `true` iff this instance is not equal to the other
     */
    bool operator!=(const PeerMsgSndr& that) const noexcept;

    /**
     * Receives messages from the socket and calls a higher-level component.
     * Doesn't return until the connection is closed. Intended to run on its own
     * thread.
     * @param[in] peerServer  Higher-level component
     */
    void runReceiver(PeerServer& peerServer) const;

    /**
     * Requests the backlog of data-chunks from the remote peer.
     * @param[in] chunkId  Identifier of earliest data-chunk in backlog
     */
    void requestBacklog(const ChunkId& chunkId) const;

    /**
     * Notifies the remote peer about available product information.
     * @param[in] prodIndex       Product index
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ProdIndex& prodIndex) const;

    /**
     * Notifies the remote peer about an available chunk-of-data.
     * @param[in] chunkId         Relevant chunk index
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ChunkId& chunkId) const;

    /**
     * Returns the number of streams.
     */
    static uint16_t getNumStreams();

    /**
     * Returns the string representation of this instance.
     * @return the string representation of this instance
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const;

    /**
     * Returns a copy of the set of requested but not-yet-received
     * chunks-of-data.
     * @return Copy of set of outstanding data-chunks
     */
    ChunkIdSet getOutstandingChunks() const;
};

} // namespace

#include <functional>

namespace std {
    template<> struct hash<hycast::PeerMsgSndr> {
        size_t operator()(const hycast::PeerMsgSndr& peer) const noexcept {
            return peer.hash();
        }
    };

    template<> struct less<hycast::PeerMsgSndr> {
        bool operator()(const hycast::PeerMsgSndr& peer1, const hycast::PeerMsgSndr& peer2)
                const noexcept {
            return peer1 < peer2;
        }
    };

    template<> struct equal_to<hycast::PeerMsgSndr> {
        bool operator()(const hycast::PeerMsgSndr& peer1, const hycast::PeerMsgSndr& peer2)
                const noexcept {
            return peer1 == peer2;
        }
    };
}

#endif /* PEER_H_ */
