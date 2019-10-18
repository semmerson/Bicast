/**
 * Keeps track of peers and chunks in a thread-safe manner.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Bookkeeper.cpp
 *  Created on: Oct 17, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "Bookkeeper.h"

#include <climits>
#include <mutex>
#include <unordered_map>

namespace hycast {

class Bookkeeper::Impl
{
    /// Concurrency stuff:
    typedef std::mutex             Mutex;
    typedef std::lock_guard<Mutex> Guard;
    mutable Mutex                  mutex;

    /// Information on a peer
    typedef struct PeerInfo {
        Bookkeeper::ChunkIds chunks;      ///< Pending/outstanding chunks
        uint_fast32_t        chunkCount;  ///< Number of received chunks
        bool                 fromConnect; ///< Resulted from `::connect()`?

        PeerInfo(const bool fromConnect)
            : chunks()
            , chunkCount{0}
            , fromConnect{fromConnect}
        {}
    } PeerInfo;

    /// Map of peer -> peer information
    std::unordered_map<Peer, PeerInfo>             peerToInfo;

    /// Map of requested chunk -> peers that have the chunk
    std::unordered_map<ChunkId, Bookkeeper::Peers> chunkToPeer;

    /// Map of remote peer address -> peer
    std::unordered_map<SockAddr, Peer>             addrToPeer;

    /**
     * Returns the IDs of the chunks that a peer has requested but that have not
     * yet been received. This instance is assumed to be locked.
     *
     * @pre                       This instance is locked
     * @param[in] peer            The peer in question
     * @return                    [first, last) iterators over the chunk IDs
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Compatible but not safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    inline std::pair<ChunkIdIter, ChunkIdIter>
    lockedGetChunkIds(const Peer& peer)
    {
        auto& chunkIds = peerToInfo.at(peer).chunks;
        return {chunkIds.begin(), chunkIds.end()};
    }

public:
    /**
     * Constructs.
     *
     * @param[in] maxPeers        Maximum number of peers
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    Impl(const int maxPeers)
        : mutex()
        , peerToInfo(maxPeers)
        , chunkToPeer()
        , addrToPeer(maxPeers)
    {}

    /**
     * Adds a peer.
     *
     * @param[in] peer            The peer
     * @param[in] fromConnect     Did the peer result from `::connect()`?
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void add(
            const Peer& peer,
            const bool  fromConnect)
    {
        Guard guard(mutex);

        peerToInfo.emplace(peer, fromConnect);
        try {
            addrToPeer.emplace(peer.getRmtAddr(), peer);
        }
        catch (...) {
            peerToInfo.erase(peer);
            throw;
        }
    }

    /**
     * Indicates if a peer resulted from a call to `::connect()`.
     *
     * @param[in] peer            The peer
     * @retval    `true`          The peer did result from a call to
     *                            `::connect()`
     * @retval    `false`         The peer did not result from a call to
     *                            `::connect()`
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    bool isFromConnect(const Peer& peer) const
    {
        Guard guard(mutex);
        return peerToInfo.at(peer).fromConnect;
    }

    /**
     * Marks a peer as having requested a particular chunk.
     *
     * @param[in] peer            The peer
     * @param[in] id              The chunk
     * @throws std::out_of_range  `peer` is unknown
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    void requested(
            const Peer&    peer,
            const ChunkId& id)
    {
        Guard guard(mutex);

        peerToInfo.at(peer).chunks.insert(id);
        chunkToPeer[id].push_back(peer);
    }

    /**
     * Indicates if a chunk has been requested by any peer.
     *
     * @param[in] id       ID of the chunk in question
     * @return    `true`   The chunk has been requested
     * @return    `false`  The chunk has not been requested
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    bool wasRequested(const ChunkId& id) noexcept
    {
        Guard guard(mutex);
        return chunkToPeer.count(id) > 0;
    }

    /**
     * Marks a chunk as having been received by a particular peer.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] id              The chunk
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void received(
            const SockAddr&  rmtAddr,
            const ChunkId&   id)
    {
        Guard guard(mutex);
        auto& peerInfo = peerToInfo.at(addrToPeer.at(rmtAddr));

        peerInfo.chunks.erase(id);
        ++peerInfo.chunkCount;

        chunkToPeer.erase(id);
    }

    /**
     * Returns the uniquely worst performing peer, which will test false if it's
     * not unique.
     *
     * @return                    The worst performing peer since construction
     *                            or `resetChunkCounts()` was called. Will test
     *                            false if the worst performing peer isn't
     *                            unique.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    Peer getWorstPeer()
    {
        unsigned long minCount{ULONG_MAX};
        unsigned long tieCount{ULONG_MAX};
        Peer          peer;
        Guard         guard(mutex);

        for (auto iter = peerToInfo.begin(), end = peerToInfo.end(); iter != end;
                ++iter) {
            auto count = iter->second.chunkCount;

            if (count < minCount) {
                minCount = count;
                peer = iter->first;
            }
            else if (count == minCount) {
                tieCount = minCount;
            }
        }

        return (minCount != tieCount) ? peer : Peer();
    }

    /**
     * Resets the count of received chunks for every peer.
     *
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    void resetChunkCounts() noexcept
    {
        Guard guard(mutex);

        for (auto iter = peerToInfo.begin(), end = peerToInfo.end(); iter != end;
                ++iter)
            iter->second.chunkCount = 0;
    }

    /**
     * Returns the IDs of the chunks that a peer has requested but that have not
     * yet been received.
     *
     * @param[in] peer            The peer in question
     * @return                    [first, last) iterators over the chunk IDs
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    std::pair<ChunkIdIter, ChunkIdIter> getChunkIds(const Peer& peer)
    {
        Guard guard(mutex);
        return lockedGetChunkIds(peer);
    }

    /**
     * Returns the peers that can request a particular chunk. The peers are in
     * the order in which they were notified.
     *
     * @param[in] chunkId         The chunk in questioni
     * @return                    [first, last) iterators over the peers
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    std::pair<PeerIter, PeerIter> getPeers(const ChunkId& chunkId)
    {
        Guard guard(mutex);
        return {chunkToPeer[chunkId].begin(), chunkToPeer[chunkId].end()};
    }

    /**
     * Removes a peer.
     *
     * @param[in] peer            The peer to be removed
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void erase(const Peer& peer)
    {
        Guard guard(mutex);
        auto  pair = lockedGetChunkIds(peer);

        for (auto chunkIdIter = pair.first; chunkIdIter != pair.second;
                ++chunkIdIter) {
            auto& peerList = chunkToPeer[*chunkIdIter];
            auto end = peerList.end();

            for (auto peerIter = peerList.begin(); peerIter != end; ++peerIter)
                if (*peerIter == peer) {
                    peerList.erase(peerIter);
                    break;
                }
        }

        addrToPeer.erase(peer.getRmtAddr());
        peerToInfo.erase(peer);
    }
};

Bookkeeper::Bookkeeper(const int maxPeers)
    : pImpl{new Impl(maxPeers)}
{}

void Bookkeeper::add(
        const Peer& peer,
        const bool  fromConnect) const
{
    pImpl->add(peer, fromConnect);
}

bool Bookkeeper::isFromConnect(const Peer& peer) const
{
    return pImpl->isFromConnect(peer);
}

void Bookkeeper::requested(
        const Peer&    peer,
        const ChunkId& id) const
{
    pImpl->requested(peer, id);
}

bool Bookkeeper::wasRequested(const ChunkId& id) const noexcept
{
    return pImpl->wasRequested(id);
}

void Bookkeeper::received(
        const SockAddr&  rmtAddr,
        const ChunkId& id) const
{
    pImpl->received(rmtAddr, id);
}

Peer Bookkeeper::getWorstPeer() const
{
    return pImpl->getWorstPeer();
}

void Bookkeeper::resetChunkCounts() const noexcept
{
    pImpl->resetChunkCounts();
}

std::pair<Bookkeeper::ChunkIdIter, Bookkeeper::ChunkIdIter>
Bookkeeper::getChunkIds(const Peer& peer) const
{
    return pImpl->getChunkIds(peer);
}

std::pair<Bookkeeper::PeerIter, Bookkeeper::PeerIter>
Bookkeeper::getPeers(const ChunkId& chunkId) const
{
    return pImpl->getPeers(chunkId);
}

void Bookkeeper::erase(const Peer& peer) const
{
    pImpl->erase(peer);
}

} // namespace
