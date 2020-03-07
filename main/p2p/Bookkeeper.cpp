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

        PeerInfo() =default;

        PeerInfo(const bool fromConnect)
            : reqChunks()
            , chunkCount{0}
            , fromConnect{fromConnect}
        {}
    } PeerInfo;

    /// Map of peer -> peer information
    std::unordered_map<Peer, PeerInfo>               peerInfos;

    /// Map of chunk identifiers -> alternative peers that can request a chunk
    std::unordered_map<ChunkId, Bookkeeper::Peers>   altPeers;

    /*
     * INVARIANTS:
     *   - If `peerInfos[peer].reqChunks` contains `chunkId`, then `peer`
     *     is not contained in `altPeers[chunkId]`
     */

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
        , peerInfos(maxPeers)
        , altPeers()
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

        peerInfos.emplace(peer, fromConnect);
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
        return peerInfos.at(peer).fromConnect;
    }

    /**
     * Indicates if a chunk should be requested by a peer. If yes, then the
     * chunk is added to the list of chunks requested by the peer; if no, then
     * the peer is added to a list of potential peers for the chunk.
     *
     * @param[in] peer               Peer
     * @param[in] chunkId            Chunk Identifier
     * @return    `true`             Chunk should be requested
     * @return    `false`            Chunk shouldn't be requested
     * @throws    std::out_of_range  Remote peer is unknown
     * @throws    logicError         Chunk has already been requested from
     *                               remote peer or remote peer is already
     *                               alternative peer for chunk
     * @threadsafety                 Safe
     * @cancellationpoint            No
     */
    bool shouldRequest(
            Peer&           peer,
            const ChunkId   chunkId)
    {
        bool  should;
        Guard guard(mutex);
        auto  elt = altPeers.find(chunkId);

        if (elt == altPeers.end()) {
            // First request for this chunk
            auto& reqChunks = peerInfos.at(peer).reqChunks;

            // Check invariant
            if (reqChunks.find(chunkId) != reqChunks.end())
                throw LOGIC_ERROR("Peer " + peer.to_string() + " has "
                        "already requested chunk " + chunkId.to_string());

            altPeers[chunkId]; // Creates empty alternative-peer list
            // Add chunk to list of chunks requested by this peer
            reqChunks.insert(chunkId);
            should = true;
        }
        else {
            auto iter = peerInfos.find(peer);

            // Check invariant
            if (iter != peerInfos.end()) {
                auto& reqChunks = iter->second.reqChunks;
                if (reqChunks.find(chunkId) != reqChunks.end())
                    throw LOGIC_ERROR("Peer " + peer.to_string() + "requested "
                            "chunk " + chunkId.to_string());
            }

            elt->second.push_back(peer); // Add alternative peer for this chunk
            should = false;
        }

        //LOG_DEBUG("Chunk %s %s be requested from %s", chunkId.to_string().data(),
                //should ? "should" : "shouldn't", peer.to_string().data());
        return should;
    }

    /**
     * Process a chunk as having been received from a peer. Nothing happens if
     * the chunk wasn't requested by the peer; otherwise, the peer is marked as
     * having received the chunk and the set of alternative peers that could but
     * haven't requested the chunk is cleared.
     *
     * @param[in] peer               Peer
     * @param[in] chunkId            Chunk Identifier
     * @retval    `false`            Chunk wasn't requested by peer.
     * @retval    `true`             Chunk was requested by peer
     * @throws    std::out_of_range  `peer` is unknown
     * @threadsafety                 Safe
     * @exceptionsafety              Basic guarantee
     * @cancellationpoint            No
     */
    bool received(
            Peer&           peer,
            const ChunkId   chunkId)
    {
        Guard  guard(mutex);
        auto&  peerInfo = peerInfos.at(peer);
        bool   wasRequested;

        if (peerInfo.reqChunks.erase(chunkId) == 0) {
            wasRequested = false;
        }
        else {
            ++peerInfo.chunkCount;
            altPeers.erase(chunkId); // Chunk is no longer relevant
            wasRequested = true;
        }

        return wasRequested;
    }

    /**
     * Returns a worst performing peer.
     *
     * @return                    A worst performing peer since construction
     *                            or `resetCounts()` was called. Will test
     *                            false if the set is empty.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    Peer getWorstPeer()
    {
        unsigned long minCount{ULONG_MAX};
        Peer          peer{};
        Guard         guard(mutex);

        for (auto elt : peerInfos) {
            auto count = elt.second.chunkCount;

            if (count < minCount) {
                minCount = count;
                peer = elt.first;
            }
        }

        return peer;
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

        for (auto& elt : peerInfos)
            elt.second.chunkCount = 0;
    }

    /**
     * Returns a reference to the identifiers of chunks that a peer has
     * requested but that have not yet been received. The set of identifiers
     * is deleted when `erase()` is called -- so the reference must not be
     * dereferenced after that.
     *
     * @param[in] peer            The peer in question
     * @return                    [first, last) iterators over the chunk
     *                            identifiers
     * @throws std::out_of_range  `peer` is unknown
     * @validity                  No changes to the peer's account
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `popBestAlt()`
     * @see                       `requested()`
     * @see                       `erase()`
     */
    Bookkeeper::ChunkIds& getRequested(const Peer& peer)
    {
        return peerInfos.at(peer).reqChunks;
    }

    /**
     * Returns the best peer to request a chunk that hasn't already requested
     * it. The peer is removed from the set of such peers.
     *
     * @param[in] chunkId         Chunk Identifier
     * @return                    The peer. Will test `false` if no such peer
     *                            exists.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `getRequested()`
     * @see                       `requested()`
     * @see                       `erase()`
     */
    Peer popBestAlt(const ChunkId chunkId)
    {
        Guard guard(mutex);
        Peer  peer{};
        auto  iter = altPeers.find(chunkId);

        if (iter != altPeers.end()) {
            auto& peers = iter->second;
            if (!peers.empty()) {
                peer = peers.front();
                peers.pop_front();
            }
        }

        return peer;
    }

    /**
     * Marks a peer as being responsible for a chunk.
     *
     * @param[in] peer     Peer
     * @param[in] chunkId  Identifier of chunk
     * @see                `getRequested()`
     * @see                `popBestAlt()`
     * @see                `erase()`
     */
    void requested(
            Peer&           peer,
            const ChunkId   chunkId)
    {
        Guard guard{mutex};
        peerInfos[peer].reqChunks.insert(chunkId);
    }

    /**
     * Removes a peer. Should be called after processing the entire set
     * returned by `getChunkIds()`.
     *
     * @param[in] peer            The peer to be removed
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `getRequested()`
     * @see                       `popBestAlt()`
     * @see                       `requested()`
     */
    void erase(const Peer& peer)
    {
        Guard    guard(mutex);

        for (auto& elt : altPeers)
            elt.second.remove(peer);

        peerInfos.erase(peer);
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

bool Bookkeeper::shouldRequest(
        Peer&           peer,
        const ChunkId   chunkId) const
{
    return pImpl->shouldRequest(peer, chunkId);
}

void Bookkeeper::received(
        Peer&           peer,
        const ChunkId   chunkId) const
{
    pImpl->received(peer, chunkId);
}

Peer Bookkeeper::getWorstPeer() const
{
    return pImpl->getWorstPeer();
}

void Bookkeeper::resetCounts() const noexcept
{
    pImpl->resetCounts();
}

Bookkeeper::ChunkIds&
Bookkeeper::getRequested(const Peer& peer) const
{
    return pImpl->getRequested(peer);
}

Peer Bookkeeper::popBestAlt(const ChunkId chunkId) const
{
    return pImpl->popBestAlt(chunkId);
}

void Bookkeeper::requested(
        Peer&           peer,
        const ChunkId   chunkId) const
{
    pImpl->requested(peer, chunkId);
}

void Bookkeeper::erase(const Peer& peer) const
{
    pImpl->erase(peer);
}

} // namespace
