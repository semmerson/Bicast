/**
 * This file implements a component that coordinates the reception of
 * data-products. Data-products are received in pieces (product-information,
 * chunks of data) via both multicast and peer-to-peer transports.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Receiving.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "P2pMgr.h"
#include "ProdStore.h"
#include "Receiving.h"

#include <mutex>
#include <unordered_set>

namespace hycast {

class Receiving::Impl final : public McastMsgRcvr, public PeerMsgRcvr
{
    ProdStore                                prodStore;
    std::unordered_set<ChunkInfo>            requestedChunks;
    std::mutex                               mutex;
    P2pMgr                                   p2pMgr;
    typedef std::lock_guard<decltype(mutex)> LockGuard;

    /**
     * Indicates if a given chunk of data should be requested. If it should,
     * then the chunk's information is added to the set of requested chunks.
     * @param[in] info   Chunk information
     * @retval `true`    Chunk should be requested. Chunk information was added
     *                   to set of requested chunks.
     * @retval `false`   Chunk should not be requested
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    bool need(ChunkInfo info)
    {
        LockGuard lock(mutex);
        if (prodStore.haveChunk(info))
            return false;
        auto                             iter = requestedChunks.find(info);
        if (iter != requestedChunks.end())
            return false;
        requestedChunks.insert(info);
        return true;
    }

    /**
     * Accepts a chunk of data. Adds it to the product-store and erases it from
     * the set of outstanding chunk-requests.
     * @param chunk      Chunk of data to accept
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    void accept(LatentChunk chunk)
    {
        LockGuard lock(mutex);
        Product   prod;
        prodStore.add(chunk, prod);
        requestedChunks.erase(chunk.getInfo());
    }

public:
    /**
     * Constructs.
     * @param[in] pathname  Pathname of product-store persistence-file or the
     *                      empty string to indicate no persistence
     * @param[in] p2pMgr    Peer-to-peer manager
     * @see ProdStore::ProdStore()
     */
    Impl(   const std::string pathname,
            P2pMgr&           p2pMgr)
        : prodStore{pathname}
        , requestedChunks{}
        , mutex{}
        , p2pMgr{p2pMgr}
    {}

    /**
     * Receives a notice about a product via multicast. Adds the information to
     * the product-store.
     * @param[in] info  Product information
     */
    void recvNotice(const ProdInfo& info)
    {
        Product prod;
        prodStore.add(info, prod);
    }

    /**
     * Receives a notice about a product from a peer. Adds the information to
     * the product-store.
     * @param[in] info  Product information
     * @param[in] peer  Peer that received the information
     */
    void recvNotice(
            const ProdInfo& info,
            Peer&           peer)
    {
        Product prod;
        prodStore.add(info, prod);
    }

    /**
     * Receives a notice about a chunk of data from a peer. If the chunk hasn't
     * already been received, then it's information is added to the set of
     * requested chunks and the chunk is requested from the peer.
     * @param[in] info  Chunk information
     * @param[in] peer  Peer that received the chunk
     */
    void recvNotice(
            const ChunkInfo& info,
            Peer&            peer)
    {
        if (need(info))
            peer.sendRequest(info);
    }

    /**
     * Receives a request for information on a product from a peer. The peer is
     * sent the information if and only if the product-store contains it.
     * @param[in] index  Product index
     * @param[in] peer   Peer that made the request
     */
    void recvRequest(
            const ProdIndex& index,
            Peer&            peer)
    {
        ProdInfo info;
        if (prodStore.getProdInfo(index, info))
            peer.sendNotice(info);
    }

    /**
     * Receives a request for a chunk of data from a peer. The peer is
     * sent the chunk if and only if the product-store contains it.
     * @param[in] info   Chunk information
     * @param[in] peer   Peer that made the request
     */
    void recvRequest(
            const ChunkInfo& info,
            Peer&            peer)
    {
        ActualChunk chunk;
        if (prodStore.getChunk(info, chunk))
            peer.sendData(chunk);
    }

    /**
     * Receives a chunk of data via multicast. A notice about the chunk is sent
     * to all peers.
     * @param[in] chunk  Chunk of data
     */
    void recvData(LatentChunk chunk)
    {
        accept(chunk);
        p2pMgr.sendNotice(chunk.getInfo());
    }

    /**
     * Receives a chunk of data from a peer. A notice about the chunk is sent
     * to all other peers.
     * @param[in] chunk  Chunk of data
     * @param[in] peer   Peer that sent the chunk
     */
    void recvData(
            LatentChunk chunk,
            Peer&       peer)
    {
        accept(chunk);
        p2pMgr.sendNotice(chunk.getInfo());
    }
};

Receiving::Receiving(
        const std::string pathname,
        P2pMgr&           p2pMgr)
    : pImpl{new Impl(pathname, p2pMgr)}
{}

void Receiving::recvNotice(const ProdInfo& info)
{
    pImpl->recvNotice(info);
}

void Receiving::recvData(LatentChunk chunk)
{
    pImpl->recvData(chunk);
}

void Receiving::recvNotice(
        const ProdInfo& info,
        Peer&           peer)
{
    pImpl->recvNotice(info, peer);
}

void Receiving::recvNotice(
        const ChunkInfo& info,
        Peer&            peer)
{
    pImpl->recvNotice(info, peer);
}

void Receiving::recvRequest(
        const ProdIndex& index,
        Peer&            peer)
{
    pImpl->recvRequest(index, peer);
}

void Receiving::recvRequest(
        const ChunkInfo& info,
        Peer&            peer)
{
    pImpl->recvRequest(info, peer);
}

void Receiving::recvData(
        LatentChunk chunk,
        Peer&       peer)
{
    pImpl->recvData(chunk, peer);
}

} /* namespace hycast */
