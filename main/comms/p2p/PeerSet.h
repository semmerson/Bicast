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
#include "Notifier.h"
#include "Peer.h"
#include "PeerSetServer.h"

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
    class                             Impl;

    std::shared_ptr<Impl>             pImpl;

public:
    /// Default maximum number of peers
    static const unsigned             defaultMaxPeers = 8;

    /*
     * Default minimum amount of time, in seconds, that the set of active peers
     * must be full and unchanged before the worst-performing peer may be
     * removed.
     */
    static const unsigned             defaultStasisDuration = 60u;

    /**
     * Constructs. The set will be empty.
     * @param[in] peerSetServer   Higher-level component
     * @param[in] maxPeers        Maximum number of peers. Default is
     *                            `PeerSet::defaultMaxPeers`.
     * @param[in] stasisDuration  Minimum amount of time, in seconds, that the
     *                            set must be full and unchanged before the
     *                            worst-performing peer may be removed. Default
     *                            is `PeerSet::defaultStasisDuration`.
     * @throws InvalidArgument    `maxPeers == 0 || stasisDuration <= 0`
     */
    PeerSet(PeerSetServer& peerSetServer,
            unsigned       maxPeers = defaultMaxPeers,
            const unsigned stasisDuration = defaultStasisDuration);

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
    bool tryInsert(PeerMsgSndr& peer) const;

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
     * Notifies all remote peers about available information on a product.
     * @param[in] prodIndex       Product index
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ProdIndex& prodIndex) const;

    /**
     * Notifies all remote peers about an available chunk-of-data.
     * @param[in] chunkId         Chunk-ID
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ChunkId& chunkId) const;

    /**
     * Increments the value of a peer.
     * @param[in] peer  Peer to have its value incremented
     */
    void incValue(PeerMsgSndr& peer) const;

    /**
     * Decrements the value of a peer.
     * @param[in] peer  Peer to have its value decremented
     */
    void decValue(PeerMsgSndr& peer) const;

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
