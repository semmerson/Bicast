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
#include "Notifier.h"
#include "ProdInfo.h"
#include "SctpSock.h"

#include <cstddef>
#include <memory>

namespace hycast {

class PeerImpl; // Forward declaration
class MsgRcvr; // Forward declaration

class Peer final : public Notifier
{
    std::shared_ptr<PeerImpl> pImpl; // `pImpl` idiom
public:
    /**
     * Constructs from nothing. Any attempt to use the resulting instance will
     * throw an exception.
     */
    Peer();

    /**
     * Constructs from an object to receive messages from the remote peer and a
     * socket. Doesn't receive anything until `runReceiver()` is called.
     * @param[in,out] msgRcvr  Object to receive messages from the remote peer
     * @param[in,out] sock     Socket
     * @see runReceiver()
     */
    Peer(MsgRcvr&   msgRcvr,
         SctpSock&  sock);

    /**
     * Returns the Internet socket address of the remote peer.
     * @return Internet socket address of remote peer
     */
    const InetSockAddr& getRemoteAddr() const;

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
    bool operator<(const Peer& that) const noexcept;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance
     * @return `true` iff this instance is equal to the other
     */
    bool operator==(const Peer& that) const noexcept;

    /**
     * Indicates if this instance is not equal to another.
     * @param[in] that  The other instance
     * @return `true` iff this instance is not equal to the other
     */
    bool operator!=(const Peer& that) const noexcept;

    /**
     * Runs the receiver. Objects are received from the socket and passed to the
     * appropriate peer-manager methods. Doesn't return until either the socket
     * is closed or an exception is thrown.
     * @throws std::logic_error  If the peer-manager didn't drain or discard the
     *                           data of a latent chunk-of-data.
     * @throws std::system_error If an I/O error occurred
     * @throws Exceptions from the peer manager
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void runReceiver() const;

    /**
     * Sends information about a product to the remote peer.
     * @param[in] prodInfo  Product information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo);

    /**
     * Sends information about a chunk-of-data to the remote peer.
     * @param[in] chunkInfo  Chunk information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo);

    /**
     * Sends a product-index to the remote peer.
     * @param[in] prodIndex  Product-index
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendRequest(const ProdIndex& prodIndex);

    /**
     * Sends a chunk specification to the remote peer.
     * @param[in] prodIndex  Product-index
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendRequest(const ChunkInfo& info);

    /**
     * Sends a chunk-of-data to the remote peer.
     * @param[in] chunk  Chunk-of-data
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendData(ActualChunk& chunk);

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
};

} // namespace

#include <functional>

namespace std {
    template<> struct hash<hycast::Peer> {
        size_t operator()(const hycast::Peer& peer) const noexcept {
            return peer.hash();
        }
    };

    template<> struct less<hycast::Peer> {
        bool operator()(const hycast::Peer& peer1, const hycast::Peer& peer2)
                const noexcept {
            return peer1 < peer2;
        }
    };

    template<> struct equal_to<hycast::Peer> {
        bool operator()(const hycast::Peer& peer1, const hycast::Peer& peer2)
                const noexcept {
            return peer1 == peer2;
        }
    };
}

#endif /* PEER_H_ */
