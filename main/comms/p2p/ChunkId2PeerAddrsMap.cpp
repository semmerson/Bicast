/**
 * This file declares a thread-safe map from a data-chunk identifier to the set
 * of peers that have the chunk.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: ChunkId2PeerAddrsMap.cpp
 *  Created on: Dec 7, 2017
 *      Author: Steven R. Emmerson
 */

#include "ChunkId2PeerAddrsMap.h"

#include <mutex>
#include <unordered_map>

namespace hycast {

class ChunkId2PeerAddrsMap::Impl final
{
    typedef std::mutex                               Mutex;
    typedef std::lock_guard<Mutex>                   LockGuard;
    typedef std::unordered_map<ChunkId, PeerAddrSet> Map;

    mutable Mutex mutex;
    Map           peerAddrs;

public:
    Impl()
        : mutex{}
        , peerAddrs{}
    {}

    size_t size() const
    {
        LockGuard lock{mutex};
        return peerAddrs.size();
    }

    void add(
            const ChunkId& chunkId,
            const InetSockAddr& peerAddr)
    {
        LockGuard lock{mutex};
        peerAddrs[chunkId].add(peerAddr);
    }

    void remove(
            const ChunkId&      chunkId,
            const InetSockAddr& peerAddr)
    {
        LockGuard lock{mutex};
        auto iter = peerAddrs.find(chunkId);
        if (iter == peerAddrs.end())
            return;
        iter->second.remove(peerAddr);
    }

    void remove(const ChunkId& chunkId)
    {
        LockGuard lock{mutex};
        peerAddrs.erase(chunkId);
    }

    bool getRandom(
            const ChunkId&              chunkId,
            InetSockAddr&               peerAddr,
            std::default_random_engine& generator) const
    {
        LockGuard lock{mutex};
        auto iter = peerAddrs.find(chunkId);
        if (iter == peerAddrs.end())
            return false;
        return iter->second.getRandom(peerAddr, generator);
    }
};

ChunkId2PeerAddrsMap::ChunkId2PeerAddrsMap()
    : pImpl{new Impl()}
{}

size_t ChunkId2PeerAddrsMap::size() const
{
    return pImpl->size();
}

void ChunkId2PeerAddrsMap::add(
        const ChunkId&      chunkId,
        const InetSockAddr& peerAddr)
{
    pImpl->add(chunkId, peerAddr);
}

void ChunkId2PeerAddrsMap::remove(
        const ChunkId&      chunkId,
        const InetSockAddr& peerAddr)
{
    pImpl->remove(chunkId, peerAddr);
}

void ChunkId2PeerAddrsMap::remove(const ChunkId& chunkId)
{
    pImpl->remove(chunkId);
}

bool ChunkId2PeerAddrsMap::getRandom(
        const ChunkId&              chunkId,
        InetSockAddr&               peerAddr,
        std::default_random_engine& generator) const
{
    return pImpl->getRandom(chunkId, peerAddr, generator);
}

} // namespace
