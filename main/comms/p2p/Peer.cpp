/**
 * This file implements a connection between peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnectionImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "Channel.h"
#include "Chunk.h"
#include "error.h"
#include "Peer.h"
#include "PeerMsgRcvr.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include "SctpSock.h"
#include "Thread.h"
#include "VersionMsg.h"

#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <thread>
#include <utility>

namespace hycast {

/// Container for all possible message types
class Peer::Message::Impl
{
    ProdIndex   prodIndex;  // Product index
    ProdInfo    prodInfo;   // Product information
    ChunkId     chunkId;    // Data-chunk identifier
    LatentChunk chunk;      // Latent chunk of data
    MsgType     type;

    Impl(const MsgType type)
        : prodIndex{}
        , prodInfo{}
        , chunkId{}
        , chunk{}
        , type{type}
    {}

public:
    Impl()                               : Impl{EMPTY}
    {}
    Impl(const ProdInfo& prodInfo)       : Impl{PROD_NOTICE}
    {
        this->prodInfo = prodInfo;
    }
    Impl(const ChunkId& chunkInfo,
         const bool       isRequest) : Impl{isRequest ? CHUNK_REQUEST
                                                      : CHUNK_NOTICE}
    {
        this->chunkId = chunkInfo;
    }
    Impl(const ProdIndex& prodIndex)     : Impl{PROD_REQUEST}
    {
        this->prodIndex = prodIndex;
    }
    Impl(const LatentChunk& chunk)       : Impl{CHUNK}
    {
        this->chunk = chunk;
    }

    Impl(const Impl& that)               =delete;
    Impl& operator =(const Message& rhs) =delete;

    operator bool() const noexcept{
        return type != EMPTY;
    }
    MsgType getType() const noexcept {
        return type;
    }

    const ProdIndex& getProdIndex() const {
        if (type != PROD_REQUEST)
            throw LOGIC_ERROR("Wrong message type " + std::to_string(type));
        return prodIndex;
    }
    const ProdInfo& getProdInfo() const {
        if (type != PROD_NOTICE)
            throw LOGIC_ERROR("Wrong message type " + std::to_string(type));
        return prodInfo;
    }
    const ChunkId& getChunkId() const {
        if (type != CHUNK_NOTICE && type != CHUNK_REQUEST)
            throw LOGIC_ERROR("Wrong message type " + std::to_string(type));
        return chunkId;
    }
    const LatentChunk& getChunk() const {
        if (type != CHUNK)
            throw LOGIC_ERROR("Wrong message type " + std::to_string(type));
        return chunk;
    }
};

Peer::Message::Message()
    : pImpl{new Impl()} {}
Peer::Message::Message(const ProdInfo& prodInfo)
    : pImpl{new Impl(prodInfo)} {}
Peer::Message::Message(const ChunkId& chunkInfo, const bool isRequest)
    : pImpl{new Impl(chunkInfo, isRequest)} {}
Peer::Message::Message(const ProdIndex& prodIndex)
    : pImpl{new Impl(prodIndex)} {}
Peer::Message::Message(const LatentChunk& chunk)
    : pImpl{new Impl(chunk)} {}
Peer::Message::operator bool() const noexcept {
    return pImpl->operator bool();
}

Peer::MsgType Peer::Message::getType() const noexcept {
    return pImpl->getType();
}

const ProdIndex& Peer::Message::getProdIndex()  const {
    return pImpl->getProdIndex();
}
const ProdInfo& Peer::Message::getProdInfo()    const {
    return pImpl->getProdInfo();
}
const ChunkId& Peer::Message::getChunkId()  const {
    return pImpl->getChunkId();
}
const LatentChunk& Peer::Message::getChunk()    const {
    return pImpl->getChunk();
}

/******************************************************************************/

class Peer::Impl final {
    typedef enum {
        VERSION_STREAM_ID = 0,
        PROD_NOTICE_STREAM_ID,
        CHUNK_NOTICE_STREAM_ID,
        PROD_REQ_STREAM_ID,
        CHUNK_REQ_STREAM_ID,
        CHUNK_STREAM_ID,
        NUM_STREAM_IDS
    }      SctpStreamId;
    unsigned                          version;
    Channel<VersionMsg>               versionChan;
    Channel<ProdInfo>                 prodNoticeChan;
    Channel<ChunkId>                  chunkNoticeChan;
    Channel<ProdIndex>                prodReqChan;
    Channel<ChunkId>                  chunkReqChan;
    Channel<ActualChunk,LatentChunk>  chunkChan;
    SctpSock                          sock;

    class : public PeerMsgRcvr
    {
        void recvNotice(const ProdInfo& info) {}
        void recvNotice(const ProdInfo& info, const Peer& peer) {}
        void recvNotice(const ChunkId& info, const Peer& peer) {}
        void recvRequest(const ProdIndex& index, const Peer& peer) {}
        void recvRequest(const ChunkId& info, const Peer& peer) {}
        void recvData(LatentChunk chunk) {}
        void recvData(LatentChunk chunk, const Peer& peer) {}
    }                      defaultMsgRcvr;

    /**
     * @throw LogicError       Unknown protocol version from remote peer
     * @throw RuntimeError     Couldn't construct peer
     */
    Impl(SctpSock&& sock)
        : Impl{std::ref<SctpSock>(sock)}
    {}

    /**
     * Returns the protocol version of the remote peer.
     * @pre                `sock.getStreamId() == VERSION_STREAM_ID`
     * @return             Protocol version of the remote peer
     * @throws LogicError  `sock.getStreamId() != VERSION_STREAM_ID`
     * @threadsafety       Safe
     */
    unsigned getVersion()
    {
        if (sock.getStreamId() != VERSION_STREAM_ID)
            throw LOGIC_ERROR("Current message isn't a version message");
        return versionChan.recv().getVersion();
    }
     /**
      * Every peer implementation is unique.
      */
     Impl(const Impl& impl) =delete;
     Impl(const Impl&& impl) =delete;
     Impl& operator=(const Impl& impl) =delete;
     Impl& operator=(const Impl&& impl) =delete;

public:
    /**
     * Default constructs. Any attempt to use use the resulting instance will
     * throw an exception.
     */
    Impl()
        : version(0),
          versionChan(),
          prodNoticeChan(),
          chunkNoticeChan(),
          prodReqChan(),
          chunkReqChan(),
          chunkChan(),
          sock()
    {}

    /**
     * Constructs from the containing peer object and a socket. Blocks
     * connecting to remote server and exchanging protocol version with remote
     * peer. This is a cancellation point.
     * @param[in] sock          Socket
     * @throw     LogicError    Unknown protocol version from remote peer
     * @throw     RuntimeError  Couldn't construct peer
     */
    Impl(SctpSock& sock)
        : version(0),
          versionChan(sock, VERSION_STREAM_ID, version),
          prodNoticeChan(sock, PROD_NOTICE_STREAM_ID, version),
          chunkNoticeChan(sock, CHUNK_NOTICE_STREAM_ID, version),
          prodReqChan(sock, PROD_REQ_STREAM_ID, version),
          chunkReqChan(sock, CHUNK_REQ_STREAM_ID, version),
          chunkChan(sock, CHUNK_STREAM_ID, version),
          sock(sock)
    {
        try {
            VersionMsg msg(version);
            versionChan.send(msg);
            const unsigned vers = getVersion();
            if (vers != version)
                throw LOGIC_ERROR(
                        "Remote peer uses unsupported protocol version: " +
                        std::to_string(vers));
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't construct peer"));
        }
    }

    /**
     * Constructs. Blocks connecting to remote server and exchanging protocol
     * version with remote peer.
     * @param[in] peerAddr      Socket address of remote peer
     * @throw     LogicError    Unknown protocol version from remote peer
     * @throw     RuntimeError  Couldn't construct peer
     * @throw     SystemError   Connection failure
     */
    Impl(const InetSockAddr& peerAddr)
        : Impl{SctpSock{getNumStreams()}.connect(peerAddr)}
    {}

    /**
     * Returns the number of streams.
     */
    static int getNumStreams()
    {
        return NUM_STREAM_IDS;
    }

    /**
     * Returns the Internet socket address of the remote peer.
     * @return Internet socket address of remote peer
     */
    InetSockAddr getRemoteAddr()
    {
        return sock.getRemoteAddr();
    }

    /**
     * Returns the next message from the remote peer.
     * @return                Next message from remote peer. Will be empty if
     *                        connection was closed by remote peer.
     * @exceptionsafety       Basic guarantee
     * @threadsafety          Thread-compatible but not thread-safe
     */
    Message getMessage()
    {
        for (;;) {
            if (sock.getSize() == 0) // Blocks waiting for input
                return Message{};
            switch (sock.getStreamId()) {
                case PROD_NOTICE_STREAM_ID:
                    return Message{prodNoticeChan.recv()};
                case CHUNK_NOTICE_STREAM_ID:
                    return Message{chunkNoticeChan.recv()};
                case PROD_REQ_STREAM_ID:
                    return Message{prodReqChan.recv()};
                case CHUNK_REQ_STREAM_ID:
                    return Message{chunkReqChan.recv(), true};
                case CHUNK_STREAM_ID: {
                    return Message{chunkChan.recv()};
                }
                default:
                    LOG_WARN("Discarding unknown message type " +
                            std::to_string(sock.getStreamId()) + " from peer " +
                            to_string());
                    sock.discard();
            }
        }
        return Message{}; // To accommodate Eclipse
    }

    /**
     * Sends information about a product to the remote peer.
     * @param[in] prodInfo  Product information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void sendProdInfo(const ProdInfo& prodInfo) const
    {
        try {
            prodNoticeChan.send(prodInfo);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send product notice "
                    + prodInfo.to_string() + " to " +
                    sock.getRemoteAddr().to_string()));
        }
    }

    /**
     * Sends information about a chunk-of-data to the remote peer.
     * @param[in] chunkId         Data-chunk identifier
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendChunkInfo(const ChunkId& chunkId)
    {
        try {
            chunkNoticeChan.send(chunkId);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send chunk notice "
                    + chunkId.to_string() + " to " +
                    sock.getRemoteAddr().to_string()));
        }
    }

    /**
     * Sends a request for product information to the remote peer.
     * @param[in] prodIndex  Product-index
     * @throws std::system_error if an I/O error occurred
     * @exceptionsafety  Basic
     * @threadsafety     Compatible but not safe
     */
    void sendProdRequest(const ProdIndex& prodIndex)
    {
        try {
            prodReqChan.send(prodIndex);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send product "
                    + prodIndex.to_string() + " request to " +
                    sock.getRemoteAddr().to_string()));
        }
    }

    /**
     * Sends a request for a chunk-of-data to the remote peer.
     * @param[in] chunkId         Data-chunk identifier
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendRequest(const ChunkId& chunkId)
    {
        try {
            chunkReqChan.send(chunkId);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send chunk request "
                    + chunkId.to_string() + " to " +
                    sock.getRemoteAddr().to_string()));
        }
    }

    /**
     * Sends a chunk-of-data to the remote peer.
     * @param[in] chunk  Chunk-of-data
     * @throws std::system_error if an I/O error occurred
     * @exceptionsafety  Basic
     * @threadsafety     Compatible but not safe
     */
    void sendData(ActualChunk& chunk)
    {
        try {
            chunkChan.send(chunk);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send data-chunk " +
                    std::to_string(chunk.getInfo()) + " to " +
                    sock.getRemoteAddr().to_string()));
        }
    }

    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @return `true` iff this instance equals the other
     * @execptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    bool operator==(const Impl& that) const noexcept {
        return this == &that; // Every instance is unique
    }

    /**
     * Indicates if this instance doesn't equal another.
     * @param[in] that  Other instance
     * @return `true` iff this instance doesn't equal the other
     * @execptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    bool operator!=(const Impl& that) const noexcept {
        return this != &that; // Every instance is unique
    }

    /**
     * Indicates if this instance is less than another.
     * @param that  Other instance
     * @return `true` iff this instance is less that the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator<(const Impl& that) const noexcept {
        return this < &that; // Every instance is unique
    }

    /**
     * Returns the hash code of this instance.
     * @return the hash code of this instance
     * @execptionsafety Nothrow
     * @threadsafety    Thread-safe
     */
    size_t hash() const noexcept {
        return std::hash<const Impl*>()(this); // Every instance is unique
    }

    /**
     * Returns the string representation of this instance.
     * @return the string representation of this instance
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const
    {
        return std::string("{addr=" + sock.getRemoteAddr().to_string() +
                ", version=" + std::to_string(version) + ", sock=" +
                sock.to_string() + "}");
    }
};

Peer::Peer()
    : pImpl(new Impl())
{}

Peer::Peer(SctpSock& sock)
    : pImpl(new Impl(sock))
{}

Peer::Peer(const InetSockAddr& peerAddr)
    : pImpl(new Impl(peerAddr))
{}

Peer::Message Peer::getMessage() const
{
    return pImpl->getMessage();
}

void Peer::sendNotice(const ProdInfo& prodInfo) const
{
    pImpl->sendProdInfo(prodInfo);
}

void Peer::sendNotice(const ChunkId& chunkInfo) const
{
    pImpl->sendChunkInfo(chunkInfo);
}

void Peer::sendRequest(const ProdIndex& prodIndex) const
{
    pImpl->sendProdRequest(prodIndex);
}

void Peer::sendRequest(const ChunkId& info) const
{
    pImpl->sendRequest(info);
}

void Peer::sendData(ActualChunk& chunk) const
{
    pImpl->sendData(chunk);
}

bool Peer::operator ==(const Peer& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

bool Peer::operator !=(const Peer& that) const noexcept
{
    return *pImpl.get() != *that.pImpl.get();
}

bool Peer::operator <(const Peer& that) const noexcept
{
    return *pImpl.get() < *that.pImpl.get();
}

uint16_t Peer::getNumStreams()
{
    return Impl::getNumStreams();
}

InetSockAddr Peer::getRemoteAddr() const
{
    return pImpl->getRemoteAddr();
}

size_t Peer::hash() const noexcept
{
    return pImpl->hash();
}

std::string Peer::to_string() const
{
    return pImpl->to_string();
}

} // namespace
