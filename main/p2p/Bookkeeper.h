/**
 * Keeps track of peers and message exchanges in a thread-safe manner.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Bookkeeper.h
 *  Created on: Oct 17, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_P2P_BOOKKEEPER_H_
#define MAIN_P2P_BOOKKEEPER_H_

#include "Peer.h"

#include <list>
#include <memory>
#include <unordered_set>
#include <utility>

namespace hycast {

class Bookkeeper
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    typedef std::unordered_set<ProdIndex> ProdIndexes;
    typedef std::unordered_set<SegId>     SegIds;
    typedef std::list<Peer>               Peers;
    typedef ProdIndexes::iterator         ProdIndexIter;
    typedef SegIds::iterator              SegIdIter;
    typedef Peers::iterator               PeerIter;

    /**
     * Constructs.
     *
     * @param[in] maxPeers        Maximum number of peers
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    Bookkeeper(int maxPeers);

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
            const bool  fromConnect) const;

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
    bool isFromConnect(const Peer& peer) const;

    /**
     * Marks a peer as having requested information on a particular product.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] prodIndex       Product index
     * @throws std::out_of_range  `peer` is unknown
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    void requested(
            const SockAddr& rmtAddr,
            const ProdIndex prodIndex) const;

    /**
     * Marks a peer as having requested a particular data-segment.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] id              The segment ID
     * @throws std::out_of_range  `peer` is unknown
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    void requested(
            const SockAddr& rmtAddr,
            const SegId&   id) const;

    /**
     * Indicates if information on a particular product has been requested by
     * any peer.
     *
     * @param[in] prodIndex  Index of the product in question
     * @return    `true`     The product-information has been requested
     * @return    `false`    The product-information has not been requested
     * @threadsafety         Safe
     * @exceptionsafety      No throw
     * @cancellationpoint    No
     */
    bool wasRequested(const ProdIndex prodIndex) const noexcept;

    /**
     * Indicates if a particular data-segment has been requested by any peer.
     *
     * @param[in] id       ID of the data-segment in question
     * @return    `true`   The data-segment has been requested
     * @return    `false`  The data-segment has not been requested
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    bool wasRequested(const SegId& id) const noexcept;

    /**
     * Marks a peer as having received information on a particular product.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] prodIndex       Index of the product
     * @throws std::out_of_range  `rmtAddr` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void received(
            const SockAddr& rmtAddr,
            const ProdIndex prodIndex) const;

    /**
     * Marks a peer as having received a particular data-segment.
     *
     * @param[in] rmtAddr         Address of the remote peer
     * @param[in] id              Data-segment ID
     * @throws std::out_of_range  `rmtAddr` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void received(
            const SockAddr& rmtAddr,
            const SegId&    id) const;

    /**
     * Returns the uniquely worst performing peer.
     *
     * @return                    The worst performing peer since construction
     *                            or `resetCounts()` was called. The returned
     *                            peer will test false if the worst performing
     *                            peer isn't unique.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    Peer getWorstPeer() const;

    /**
     * Resets the measure of utility for every peer.
     *
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    void resetCounts() const noexcept;

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
            const;

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
    std::pair<SegIdIter, SegIdIter> getSegIds(const Peer& peer) const;

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
    void erase(const Peer& peer) const;

    /**
     * Returns the best local peer to request information on a particular
     * product and that isn't a particular peer.
     *
     * @param[in] prodIndex       Index of the product
     * @param[in] except          Peer to avoid
     * @return                    The peer. Will test `false` if no such peer
     *                            exists.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    Peer getBestPeerExcept(
            const ProdIndex prodIndex,
            const Peer&     except) const;

    /**
     * Returns the local peer to request a particular data-segment and
     * that isn't a particular peer.
     *
     * @param[in] segId           Segment ID
     * @param[in] except          Peer to avoid
     * @return                    The peer. Will test `false` if no such peer
     *                            exists.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    Peer getBestPeerExcept(
            const SegId& segId,
            const Peer&  except) const;
};

} // namespace

#endif /* MAIN_P2P_BOOKKEEPER_H_ */
