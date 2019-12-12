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
        /// Pending/outstanding product-information
        Bookkeeper::ProdIndexes reqProdInfos;
        /// Pending/outstanding data-segments
        Bookkeeper::SegIds      reqSegs;
        uint_fast32_t           chunkCount;  ///< Number of received messages
        bool                    fromConnect; ///< Resulted from `::connect()`?

        PeerInfo(const bool fromConnect)
            : reqSegs()
            , chunkCount{0}
            , fromConnect{fromConnect}
        {}
    } PeerInfo;

    /// Map of peer -> peer information
    std::unordered_map<Peer, PeerInfo>               lclPeerInfos;

    /// Map of requested product-information -> local peers that can request the
    /// information
    std::unordered_map<ProdIndex, Bookkeeper::Peers> potInfoPeers;

    /// Map of requested segment -> local peers that can request the segment
    std::unordered_map<SegId, Bookkeeper::Peers>     potSegPeers;

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
        , potInfoPeers()
        , potSegPeers()
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
     * Marks a local peer as having requested information on a particular
     * product.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] prodIndex       Index of the product
     * @throws std::out_of_range  `peer` is unknown
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    void requested(
            const SockAddr& rmtAddr,
            const ProdIndex prodIndex)
    {
        Guard guard(mutex);

        Peer& peer = lclPeers.at(rmtAddr);
        lclPeerInfos.at(peer).reqProdInfos.insert(prodIndex);
        potInfoPeers[prodIndex].push_back(peer);
    }

    /**
     * Marks a peer as having requested a particular data-segment.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] segId           The segment ID
     * @throws std::out_of_range  `peer` is unknown
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    void requested(
            const SockAddr& rmtAddr,
            const SegId&    id)
    {
        Guard guard(mutex);

        Peer& peer = lclPeers.at(rmtAddr);
        lclPeerInfos.at(peer).reqSegs.insert(id);
        potSegPeers[id].push_back(peer);
    }

    /**
     * Indicates if product-information has been requested by any local peer.
     *
     * @param[in] prodIndex  Index of the product
     * @return    `true`     The data-segment has been requested
     * @return    `false`    The data-segment has not been requested
     * @threadsafety         Safe
     * @exceptionsafety      No throw
     * @cancellationpoint    No
     */
    bool wasRequested(const ProdIndex prodIndex) noexcept
    {
        Guard guard(mutex);
        return potInfoPeers.count(prodIndex) > 0;
    }

    /**
     * Indicates if a data-segment has been requested by any peer.
     *
     * @param[in] id       ID of the data-segment in question
     * @return    `true`   The data-segment has been requested
     * @return    `false`  The data-segment has not been requested
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    bool wasRequested(const SegId& id) noexcept
    {
        Guard guard(mutex);
        return potSegPeers.count(id) > 0;
    }

    /**
     * Marks product-information as having been received from a remote peer.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] prodIndex       Index of the data-product
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void received(
            const SockAddr& rmtAddr,
            const ProdIndex prodIndex)
    {
        Guard guard(mutex);
        auto& peerInfo = lclPeerInfos.at(lclPeers.at(rmtAddr));

        peerInfo.reqProdInfos.erase(prodIndex);
        ++peerInfo.chunkCount;

        potInfoPeers.erase(prodIndex); // Product-information is no longer relevant
    }

    /**
     * Marks a data-segment as having been received by a particular peer.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] id              Data-segment ID
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void received(
            const SockAddr& rmtAddr,
            const SegId&    id)
    {
        Guard guard(mutex);
        auto& peerInfo = lclPeerInfos.at(lclPeers.at(rmtAddr));

        peerInfo.reqSegs.erase(id);
        ++peerInfo.chunkCount;

        potSegPeers.erase(id); // Segment is no longer relevant
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

        for (auto iter = lclPeerInfos.begin(), end = lclPeerInfos.end(); iter != end;
                ++iter)
            iter->second.chunkCount = 0;
    }

    /**
     * Returns the indexes of products that a peer has requested information on
     * but that have not yet been received. Should be called before `erase()`.
     *
     * @param[in] peer            The peer in question
     * @return                    [first, last) iterators over the product
     *                            indexes
     * @throws std::out_of_range  `peer` is unknown
     * @validity                  No changes to the peer's account
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `erase()`
     */
    std::pair<ProdIndexIter, ProdIndexIter> getProdIndexes(const Peer& peer)
    {
        Guard guard(mutex);
        auto& prodIndexes = lclPeerInfos.at(peer).reqProdInfos;
        return {prodIndexes.begin(), prodIndexes.end()};
    }

    /**
     * Returns the IDs of the data-segments that a peer has requested but that
     * have not yet been received. Should be called before `erase()`.
     *
     * @param[in] peer            The peer in question
     * @return                    [first, last) iterators over the segment IDs
     * @throws std::out_of_range  `peer` is unknown
     * @validity                  No changes to the peer's account
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `erase()`
     */
    std::pair<SegIdIter, SegIdIter> getSegIds(const Peer& peer)
    {
        Guard guard(mutex);
        auto& segIds = lclPeerInfos.at(peer).reqSegs;
        return {segIds.begin(), segIds.end()};
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

        {
            auto& segIds = peerInfo.reqSegs;
            auto  end = segIds.end();

            for (auto segIdIter = segIds.begin(); segIdIter != end;
                    ++segIdIter) {
                auto& peerList = potSegPeers[*segIdIter];
                auto  end = peerList.end();

                for (auto peerIter = peerList.begin(); peerIter != end;
                        ++peerIter)
                    if (*peerIter == peer) {
                        peerList.erase(peerIter);
                        break;
                    }
            }
        }

        {
            auto& prodIndexes = peerInfo.reqProdInfos;
            auto  end = prodIndexes.end();

            for (auto indexIter = prodIndexes.begin(); indexIter != end;
                    ++indexIter) {
                auto& peerList = potInfoPeers[*indexIter];
                auto  end = peerList.end();

                for (auto peerIter = peerList.begin(); peerIter != end;
                        ++peerIter)
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
     * Returns the best local peer to request information on a particular
     * product and that's not a particular peer.
     *
     * @param[in] prodIndex       Index of the product
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
            const ProdIndex prodIndex,
            const Peer&     except)
    {
        Guard guard(mutex);
        for (auto& peer : potInfoPeers[prodIndex]) {
            if (peer == except)
                continue;
            return peer;
        }
        return Peer{};
    }

    /**
     * Returns the local peer that can request a particular data-segment and
     * was notified of the segment before all the other peers. Should be called
     * after `erase()`.
     *
     * @param[in] segId           Segment ID
     * @return                    The peer. Will test `false` if no such peer
     *                            exists.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `erase()`
     */
    Peer getBestPeerExcept(
            const SegId& id,
            const Peer&  except)
    {
        Guard guard(mutex);
        for (auto& peer : potSegPeers[id]) {
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
        const ProdIndex prodIndex) const
{
    pImpl->requested(rmtAddr, prodIndex);
}

void Bookkeeper::requested(
        const SockAddr& rmtAddr,
        const SegId&    id) const
{
    pImpl->requested(rmtAddr, id);
}

bool Bookkeeper::wasRequested(const ProdIndex prodIndex) const noexcept
{
    return pImpl->wasRequested(prodIndex);
}

bool Bookkeeper::wasRequested(const SegId& id) const noexcept
{
    return pImpl->wasRequested(id);
}

void Bookkeeper::received(
        const SockAddr& rmtAddr,
        const ProdIndex prodIndex) const
{
    pImpl->received(rmtAddr, prodIndex);
}

void Bookkeeper::received(
        const SockAddr&  rmtAddr,
        const SegId&     id) const
{
    pImpl->received(rmtAddr, id);
}

Peer Bookkeeper::getWorstPeer() const
{
    return pImpl->getWorstPeer();
}

void Bookkeeper::resetCounts() const noexcept
{
    pImpl->resetCounts();
}

std::pair<Bookkeeper::ProdIndexIter, Bookkeeper::ProdIndexIter>
Bookkeeper::getProdIndexes(const Peer& peer) const
{
    return pImpl->getProdIndexes(peer);
}

std::pair<Bookkeeper::SegIdIter, Bookkeeper::SegIdIter>
Bookkeeper::getSegIds(const Peer& peer) const
{
    return pImpl->getSegIds(peer);
}

void Bookkeeper::erase(const Peer& peer) const
{
    pImpl->erase(peer);
}

Peer Bookkeeper::getBestPeerExcept(
        const ProdIndex prodIndex,
        const Peer&     except) const
{
    return pImpl->getBestPeerExcept(prodIndex, except);
}

Peer Bookkeeper::getBestPeerExcept(
        const SegId& id,
        const Peer&  except) const
{
    return pImpl->getBestPeerExcept(id, except);
}

} // namespace
