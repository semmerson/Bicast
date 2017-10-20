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

class Peer final : public Notifier
{
    class                 Impl;  // Forward declaration
    std::shared_ptr<Impl> pImpl; // `pImpl` idiom

public:
    /// Types of messages exchanged by peers
    typedef enum {
        EMPTY,         // Empty message
        PROD_NOTICE,   // Notice of product
        CHUNK_NOTICE,  // Notice of available chunk of data
        PROD_REQUEST,  // Request for product information
        CHUNK_REQUEST, // Request for chunk of data
        CHUNK          // Chunk of data
    } MsgType;

    /// Union of all possible message types
    class Message
    {
        class                 Impl;
        std::shared_ptr<Impl> pImpl;
    public:
        Message();
        explicit Message(const ProdInfo& prodInfo);
        explicit Message(const ChunkInfo& chunkInfo,
                         const bool       isRequest = false);
        explicit Message(const ProdIndex& prodIndex);
        explicit Message(const LatentChunk& chunk);

        operator bool() const noexcept;
        MsgType getType() const noexcept;

        const ProdInfo& getProdInfo() const;
        const ChunkInfo& getChunkInfo() const;
        const ProdIndex& getProdIndex() const;
        const LatentChunk& getChunk() const;
    };

    void* getPimpl() const {
        return pImpl.get();
    }

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
    Peer(SctpSock& sock);

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
     * Returns the next message from the remote peer.
     * @return           Next message from remote peer
     * @exceptionsafety  Basic guarantee
     * @threadsafefy     Thread-compatible but not thread-safe
     */
    Message getMessage() const;

    /**
     * Sends information about a product to the remote peer.
     * @param[in] prodInfo  Product information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo) const;

    /**
     * Sends information about a chunk-of-data to the remote peer.
     * @param[in] chunkInfo  Chunk information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo) const;

    /**
     * Sends a product-index to the remote peer.
     * @param[in] prodIndex  Product-index
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendRequest(const ProdIndex& prodIndex) const;

    /**
     * Sends a chunk specification to the remote peer.
     * @param[in] prodIndex  Product-index
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendRequest(const ChunkInfo& info) const;

    /**
     * Sends a chunk-of-data to the remote peer.
     * @param[in] chunk  Chunk-of-data
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendData(ActualChunk& chunk) const;

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
