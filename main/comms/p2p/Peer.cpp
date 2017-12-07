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
#include "PeerServer.h"
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

class Peer::Impl final {
    typedef enum {
        VERSION_STREAM_ID = 0,
        BACKLOG_STREAM_ID,
        PROD_NOTICE_STREAM_ID,
        PROD_INFO_STREAM_ID,
        CHUNK_NOTICE_STREAM_ID,
        PROD_REQ_STREAM_ID,
        CHUNK_REQ_STREAM_ID,
        CHUNK_STREAM_ID,
        NUM_STREAM_IDS
    }      SctpStreamId;
    unsigned                          version;
    Channel<VersionMsg>               versionChan;
    Channel<ChunkId>                  backlogChan;
    Channel<ProdIndex>                prodNoticeChan;
    Channel<ProdInfo>                 prodInfoChan;
    Channel<ChunkId>                  chunkNoticeChan;
    Channel<ProdIndex>                prodReqChan;
    Channel<ChunkId>                  chunkReqChan;
    Channel<ActualChunk,LatentChunk>  chunkChan;
    SctpSock                          sock;

    /**
     * Every peer implementation is unique.
     */
    Impl(const Impl& impl) =delete;
    Impl(const Impl&& impl) =delete;
    Impl& operator=(const Impl& impl) =delete;
    Impl& operator=(const Impl&& impl) =delete;

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

    void sendVersion()
    {
        VersionMsg msg(version);
        versionChan.send(msg);
    }

public:
    /**
     * Default constructs. Any attempt to use use the resulting instance will
     * throw an exception.
     */
    Impl()
        : version(0),
          versionChan{},
          backlogChan{},
          prodNoticeChan{},
          chunkNoticeChan{},
          prodReqChan{},
          chunkReqChan{},
          chunkChan{},
          sock{}
    {}

    /**
     * Constructs. Blocks connecting to remote server and exchanging protocol
     * version with remote peer. This is a cancellation point.
     * @param[in] sock          Socket
     * @throw     LogicError    Unknown protocol version from remote peer
     * @throw     RuntimeError  Couldn't construct peer
     */
    Impl(   SctpSock&    sock)
        : version(0),
          versionChan(sock, VERSION_STREAM_ID, version),
          backlogChan(sock, BACKLOG_STREAM_ID, version),
          prodNoticeChan(sock, PROD_NOTICE_STREAM_ID, version),
          prodInfoChan(sock, PROD_INFO_STREAM_ID, version),
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
    Impl(   const InetSockAddr& peerAddr)
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
    inline InetSockAddr getRemoteAddr() const
    {
        return sock.getRemoteAddr();
    }

    void runReceiver(PeerServer& peerServer)
    {
        try {
            while (sock.getSize()) { // Blocks waiting for input
                switch (sock.getStreamId()) {
                    case BACKLOG_STREAM_ID: {
                        auto chunkId = backlogChan.recv();
                        peerServer.startBacklog(chunkId);
                        break;
                    }
                    case PROD_NOTICE_STREAM_ID: {
                        auto prodIndex = prodNoticeChan.recv();
                        if (peerServer.shouldRequest(prodIndex))
                            request(prodIndex);
                        break;
                    }
                    case CHUNK_NOTICE_STREAM_ID: {
                        auto chunkId = chunkNoticeChan.recv();
                        LOG_DEBUG("Received notice of chunk " +
                                chunkId.to_string());
                        if (peerServer.shouldRequest(chunkId))
                            request(chunkId);
                        break;
                    }
                    case PROD_REQ_STREAM_ID: {
                        ProdInfo prodInfo{};
                        if (peerServer.get(prodReqChan.recv(), prodInfo))
                            send(prodInfo);
                        break;
                    }
                    case CHUNK_REQ_STREAM_ID: {
                        ActualChunk chunk;
                        if (peerServer.get(chunkReqChan.recv(), chunk))
                            send(chunk);
                        break;
                    }
                    case PROD_INFO_STREAM_ID: {
                        auto prodInfo = prodInfoChan.recv();
                        peerServer.receive(prodInfo);
                        break;
                    }
                    case CHUNK_STREAM_ID: {
                        auto chunk = chunkChan.recv();
                        peerServer.receive(chunk);
                        break;
                    }
                    default:
                        LOG_WARN("Discarding unknown message type " +
                                std::to_string(sock.getStreamId()) +
                                " from remote peer " + to_string());
                        sock.discard();
                } // Channel/Stream-ID switch
            } // Input loop
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Failure of peer " +
                    to_string()));
        }
    }

    void requestBacklog(const ChunkId& chunkId)
    {
        backlogChan.send(chunkId);
    }

    /**
     * Notifies the remote peer about available information on a product.
     * @param[in] prodIndex       Index of the relevant product
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ProdIndex& prodIndex) const
    {
        try {
            prodNoticeChan.send(prodIndex);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't notify remote peer "
                    + getRemoteAddr().to_string() +
                    " about information on product " + prodIndex.to_string()));
        }
    }

    /**
     * Notifies the remote peer about an available chunk of data.
     * @param[in] chunkId         Relevant data-chunk identifier
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ChunkId& chunkId) const
    {
        try {
            chunkNoticeChan.send(chunkId);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't notify remote peer "
                    + getRemoteAddr().to_string() +
                    " about data-chunk " + chunkId.to_string()));
        }
    }

    /**
     * Requests information on a product from the remote peer.
     * @param[in] prodIndex       Index of relevant product
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void request(const ProdIndex& prodIndex) const
    {
        try {
            prodReqChan.send(prodIndex);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR(
                    "Couldn't request information on product "
                    + prodIndex.to_string() + " from remote peer " +
                    getRemoteAddr().to_string()));
        }
    }

    /**
     * Requests a chunk-of-data from the remote peer.
     * @param[in] chunkId         Data-chunk identifier
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void request(const ChunkId& chunkId)
    {
        try {
            chunkReqChan.send(chunkId);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't request data-chunk "
                    + chunkId.to_string() + " from remote peer " +
                    getRemoteAddr().to_string()));
        }
    }

    /**
     * Sends information on a product to the remote peer.
     * @param[in] prodInfo        Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void send(const ProdInfo& prodInfo) const
    {
        try {
            prodInfoChan.send(prodInfo);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR(
                    "Couldn't send information on product " +
                    prodInfo.to_string() + " to remote peer " +
                    getRemoteAddr().to_string()));
        }
    }

    /**
     * Sends a chunk of data to the remote peer.
     * @param[in] chunk           Chunk of data
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void send(const ActualChunk& chunk) const
    {
        try {
            chunkChan.send(chunk);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send data-chunk " +
                    chunk.getId().to_string() + " to remote peer " +
                    getRemoteAddr().to_string()));
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
    : pImpl{new Impl(sock)}
{}

Peer::Peer(const InetSockAddr& peerAddr)
    : pImpl{new Impl(peerAddr)}
{}

void Peer::runReceiver(PeerServer& peerServer) const
{
    pImpl->runReceiver(peerServer);
}

void Peer::requestBacklog(const ChunkId& chunkId) const
{
    pImpl->requestBacklog(chunkId);
}

void Peer::notify(const ProdIndex& prodIndex) const
{
    pImpl->notify(prodIndex);
}

void Peer::notify(const ChunkId& chunkId) const
{
    pImpl->notify(chunkId);
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
