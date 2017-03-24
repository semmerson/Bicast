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
    ProdStore                     prodStore;
    std::unordered_set<ChunkInfo> requestedChunks;
    std::mutex                    mutex;
    P2pMgr                        p2pMgr;

    /**
     * Indicates if a given chunk of data should be requested. Adds the chunk's
     * information to the set of requested chunks if it should be requested.
     * @param[in] info  Chunk information
     * @retval `true`   Chunk should be requested. Chunk information was added
     *                  to set of requested chunks.
     * @retval `false`  Chunk should not be requested
     */
    bool need(ChunkInfo info)
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        if (prodStore.haveChunk(info))
            return false;
        auto                             iter = requestedChunks.find(info);
        if (iter != requestedChunks.end())
            return false;
        requestedChunks.insert(info);
        return true;
    }

public:
    /**
     * Constructs.
     * @param[in] pathname  Pathname of product-store persistence-file or the
     *                      empty string to indicate no persistence
     * @see ProdStore::ProdStore()
     */
    Impl(const std::string pathname, P2pMgr& p2pMgr)
        : prodStore{pathname}
        , p2pMgr{p2pMgr}
    {}

    void recvNotice(const ProdInfo& info)
    {
        Product prod;
        prodStore.add(info, prod);
    }

    void recvData(LatentChunk chunk)
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        Product                          prod;
        prodStore.add(chunk, prod);
        auto chunkInfo = chunk.getInfo();
        requestedChunks.erase(chunkInfo);
        p2pMgr.sendNotice(chunkInfo);
    }

    void recvNotice(const ProdInfo& info, Peer& peer)
    {
        Product prod;
        prodStore.add(info, prod);
    }

    void recvNotice(const ChunkInfo& info, Peer& peer)
    {
        if (need(info))
            peer.sendRequest(info);
    }

    void recvRequest(const ProdIndex& index, Peer& peer)
    {
        ProdInfo info;
        if (prodStore.getProdInfo(index, info))
            peer.sendNotice(info);
    }

    void recvRequest(const ChunkInfo& info, Peer& peer)
    {
        ActualChunk chunk;
        if (prodStore.getChunk(info, chunk))
            peer.sendData(chunk);
    }

    void recvData(LatentChunk chunk, Peer& peer)
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        Product prod;
        prodStore.add(chunk, prod);
        auto chunkInfo = chunk.getInfo();
        requestedChunks.erase(chunkInfo);
        p2pMgr.sendNotice(chunkInfo, peer);
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

void Receiving::recvNotice(const ProdInfo& info, Peer& peer)
{
    pImpl->recvNotice(info, peer);
}

void Receiving::recvNotice(const ChunkInfo& info, Peer& peer)
{
    pImpl->recvNotice(info, peer);
}

void Receiving::recvRequest(const ProdIndex& index, Peer& peer)
{
    pImpl->recvRequest(index, peer);
}

void Receiving::recvRequest(const ChunkInfo& info, Peer& peer)
{
    pImpl->recvRequest(info, peer);
}

void Receiving::recvData(LatentChunk chunk, Peer& peer)
{
    pImpl->recvData(chunk, peer);
}

} /* namespace hycast */
