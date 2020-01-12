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
        /// Requested chunks that haven't been received
        Bookkeeper::ChunkIds    reqChunks;
        uint_fast32_t           chunkCount;  ///< Number of received messages
        bool                    fromConnect; ///< Resulted from `::connect()`?

        PeerInfo(const bool fromConnect)
            : reqChunks()
            , chunkCount{0}
            , fromConnect{fromConnect}
        {}
    } PeerInfo;

    /// Map of peer -> peer information
    std::unordered_map<Peer, PeerInfo>               lclPeerInfos;

    /// Map of chunk identifiers -> local peers that can request the chunks
    std::unordered_map<ChunkId, Bookkeeper::Peers>   willingPeers;

    /// Map of remote peer address -> peer
    std::unordered_map<SockAddr, Peer>               lclPeers;

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
        , lclPeerInfos(maxPeers)
        , willingPeers()
        , lclPeers(maxPeers)
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

        lclPeerInfos.emplace(peer, fromConnect);
        try {
            lclPeers.emplace(peer.getRmtAddr(), peer);
        }
        catch (...) {
            lclPeerInfos.erase(peer);
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
        return lclPeerInfos.at(peer).fromConnect;
    }

    /**
     * Marks a local peer as having requested a particular chunk.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] chunkId         Chunk Identifier
     * @throws std::out_of_range  `peer` is unknown
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    void requested(
            const SockAddr& rmtAddr,
            const ChunkId   chunkId)
    {
        Guard guard(mutex);

        Peer& peer = lclPeers.at(rmtAddr);
        lclPeerInfos.at(peer).reqChunks.insert(chunkId);
        willingPeers[chunkId].push_back(peer);
    }

    /**
     * Indicates if a chunk has been requested by any local peer.
     *
     * @param[in] chunkId    Chunk Identifier
     * @return    `true`     The data-segment has been requested
     * @return    `false`    The data-segment has not been requested
     * @threadsafety         Safe
     * @exceptionsafety      No throw
     * @cancellationpoint    No
     */
    bool wasRequested(const ChunkId chunkId) noexcept
    {
        Guard guard(mutex);
        return willingPeers.count(chunkId) > 0;
    }

    /**
     * Marks a chunk as having been received from a remote peer.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] chunkId         Chunk Identifier
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void received(
            const SockAddr& rmtAddr,
            const ChunkId   chunkId)
    {
        Guard guard(mutex);
        auto& peerInfo = lclPeerInfos.at(lclPeers.at(rmtAddr));

        peerInfo.reqChunks.erase(chunkId);
        ++peerInfo.chunkCount;

        willingPeers.erase(chunkId); // Chunk is no longer relevant
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

        for (auto iter = lclPeerInfos.begin(), end = lclPeerInfos.end(); iter != end;
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
    void resetCounts() noexcept
    {
        Guard guard(mutex);

        for (auto iter = lclPeerInfos.begin(), end = lclPeerInfos.end();
                iter != end; ++iter)
            iter->second.chunkCount = 0;
    }

    /**
     * Returns the identifiers of chunks that a peer has requested but that
     * have not yet been received. Should be called before `erase()`.
     *
     * @param[in] peer            The peer in question
     * @return                    [first, last) iterators over the chunk
     *                            identifiers
     * @throws std::out_of_range  `peer` is unknown
     * @validity                  No changes to the peer's account
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `erase()`
     */
    std::pair<ChunkIdIter, ChunkIdIter> getChunkIds(const Peer& peer)
    {
        Guard guard(mutex);
        auto& chunkIds = lclPeerInfos.at(peer).reqChunks;
        return {chunkIds.begin(), chunkIds.end()};
    }

    /**
     * Removes a peer. Should be called after `getProdIndexes()` and
     * `getSegIds()` and before `getBestPeer()`.
     *
     * @param[in] peer            The peer to be removed
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `getProdIndexes()`
     * @see                       `getSegIds()`
     * @see                       `getBestPeer()`
     */
    void erase(const Peer& peer)
    {
        Guard    guard(mutex);
        PeerInfo peerInfo = lclPeerInfos.at(peer);
        auto&    chunkIds = peerInfo.reqChunks;
        auto     end = chunkIds.end();

        for (auto segIdIter = chunkIds.begin(); segIdIter != end; ++segIdIter) {
            auto& peerList = willingPeers[*segIdIter];
            auto  end = peerList.end();

            for (auto peerIter = peerList.begin(); peerIter != end;
                    ++peerIter) {
                if (*peerIter == peer) {
                    peerList.erase(peerIter);
                    break;
                }
            }
        }

        lclPeers.erase(peer.getRmtAddr());
        lclPeerInfos.erase(peer);
    }

    /**
     * Returns the best local peer to request a chunk that's not a particular
     * peer.
     *
     * @param[in] chunkId         Chunk Identifier
     * @param[in] except          Peer to avoid
     * @return                    The peer. Will test `false` if no such peer
     *                            exists.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `erase()`
     */
    Peer getBestPeerExcept(
            const ChunkId chunkId,
            const Peer&   except)
    {
        Guard guard(mutex);
        for (auto& peer : willingPeers[chunkId]) {
            if (peer == except)
                continue;
            return peer;
        }
        return Peer{};
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
        const SockAddr& rmtAddr,
        const ChunkId chunkId) const
{
    pImpl->requested(rmtAddr, chunkId);
}

bool Bookkeeper::wasRequested(const ChunkId chunkId) const noexcept
{
    return pImpl->wasRequested(chunkId);
}

void Bookkeeper::received(
        const SockAddr& rmtAddr,
        const ChunkId   chunkId) const
{
    pImpl->received(rmtAddr, chunkId);
}

Peer Bookkeeper::getWorstPeer() const
{
    return pImpl->getWorstPeer();
}

void Bookkeeper::resetCounts() const noexcept
{
    pImpl->resetCounts();
}

std::pair<Bookkeeper::ChunkIdIter, Bookkeeper::ChunkIdIter>
Bookkeeper::getChunkIds(const Peer& peer) const
{
    return pImpl->getChunkIds(peer);
}

void Bookkeeper::erase(const Peer& peer) const
{
    pImpl->erase(peer);
}

Peer Bookkeeper::getBestPeerExcept(
        const ChunkId chunkId,
        const Peer&   except) const
{
    return pImpl->getBestPeerExcept(chunkId, except);
}

} // namespace
