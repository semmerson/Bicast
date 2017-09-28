/**
 * This file declares a set of peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSet.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERSET_H_
#define PEERSET_H_

#include "InetSockAddr.h"
#include "MsgRcvr.h"
#include "Notifier.h"
#include "Peer.h"
#include "ProdStore.h"

#include <chrono>
#include <functional>
#include <memory>

namespace hycast {

class PeerSetInfo final
{
public:
	PeerSetInfo(
			const unsigned       maxPeers,
			const unsigned       stasisDuration);
};

class PeerSet final : public Notifier
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    typedef std::chrono::seconds      TimeUnit;
    typedef std::chrono::steady_clock Clock;

    /// Default maximum number of peers
    static const unsigned             defaultMaxPeers = 8;

    /**
     * Constructs. The set will be empty.
     * @param[in] prodStore       Product storage
     * @param[in] stasisDuration  Minimum amount of time that the set must be
     *                            full and unchanged before the worst-performing
     *                            peer may be removed
     * @param[in] maxPeers        Maximum number of peers. Default is
     *                            `PeerSet::defaultMaxPeers`.
     * @param[in] peerStopped     Function to call when a peer stops. Default
     *                            does nothing.
     * @throws InvalidArgument  ` maxPeers == 0 || stasisDuration <= 0`
     */
    explicit PeerSet(
            ProdStore&                         prodStore,
            const TimeUnit                     stasisDuration,
            unsigned                           maxPeers = defaultMaxPeers,
            std::function<void(InetSockAddr&)> peerStopped =
                    [](InetSockAddr&){});

    /**
     * Constructs. The set will be empty.
     * @param[in] prodStore       Product storage
     * @param[in] statisDuration  Minimum amount of time, in units of
     *                            `PeerSet::TimeUnit`, that the set must be full
     *                            and unchanged before the worst-performing peer
     *                            may be removed
     * @param[in] maxPeers        Maximum number of peers. Default is
     *                            `PeerSet::defaultMaxPeers`.
     * @param[in] peerStopped     Function to call when a peer stops. Default
     *                            does nothing.
     * @throws InvalidArgument  ` maxPeers == 0 || stasisDuration <= 0`
     */
    explicit PeerSet(
            ProdStore&                         prodStore,
            const unsigned                     stasisDuration,
            unsigned                           maxPeers = defaultMaxPeers,
            std::function<void(InetSockAddr&)> peerStopped =
                    [](InetSockAddr&){})
        : PeerSet{prodStore, TimeUnit{stasisDuration}, maxPeers, peerStopped}
    {}

    /**
     * Tries to insert a peer. The attempt will fail if the peer is already a
     * member. If the set is full, then
     *   - The current thread is blocked until the membership has been unchanged
     *     for at least the amount of time given to the constructor; and
     *   - The worst-performing will have been removed from the set.
     * @param[in]  peer     Candidate peer
     * @return     `false`  Peer is already a member
     * @return     `true`   Peer was added. Worst peer removed and reported if
     *                      the set was full.
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    bool tryInsert(Peer& peer) const;

    /**
     * Indicates if this instance already has a given remote peer.
     * @param[in] peerAddr  Address of the remote peer
     * @retval `true`       The peer is a member
     * @retval `false`      The peer is not a member
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    bool contains(const InetSockAddr& peerAddr) const;

    /**
     * Sends information about a product to all peers in the set.
     * @param[in] prodInfo        Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo) const;

    /**
     * Sends information about a product to all peers in the set except one.
     * @param[in] prodInfo        Product information
     * @param[in] except          Address of peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo, const InetSockAddr& except) const;

    /**
     * Sends information about a chunk-of-data to all peers in the set.
     * @param[in] chunkInfo       Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo) const;

    /**
     * Sends information about a chunk-of-data to all peers in the set except
     * one.
     * @param[in] chunkInfo       Chunk information
     * @param[in] except          Address of peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo, const InetSockAddr& except) const;

    /**
     * Increments the value of a peer.
     * @param[in] peer  Peer to have its value incremented
     */
    void incValue(Peer& peer) const;

    /**
     * Decrements the value of a peer.
     * @param[in] peer  Peer to have its value decremented
     */
    void decValue(Peer& peer) const;

    /**
     * Returns the number of peers in the set.
     * @return Number of peers in the set
     */
    size_t size() const;

    /**
     * Indicates if this instance is full.
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    bool isFull() const;
};

} // namespace

#endif /* PEERSET_H_ */
