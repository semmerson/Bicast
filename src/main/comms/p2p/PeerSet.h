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

#include <memory>

namespace hycast {

class PeerSet final : public Notifier
{
    class Impl;                  /// Forward declaration
    std::shared_ptr<Impl> pImpl; /// `pImpl` idiom

public:
    typedef enum {
        EXISTS,    /// Peer is already member of set
        SUCCESS,   /// Success
        REPLACED,  /// Success. Inserted peer replaced worst-performing member
        FULL       /// Set is full and insufficient time to determine worst peer
    } InsertStatus;

    /**
     * Constructs from the maximum number of peers. The set will be empty.
     * @param[in] peerTerminated      Function to call when a peer terminates
     * @param[in] maxPeers            Maximum number of peers
     * @param[in] stasisDuration      Required duration, in seconds, without
     *                                change to the set of peers before the
     *                                worst-performing peer may be replaced
     * @throws std::invalid_argument  `maxPeers == 0`
     */
    explicit PeerSet(
            std::function<void(Peer&)> peerTerminated,
            unsigned                   maxPeers = 8,
            unsigned                   stasisDuration = 60);

    /**
     * Tries to insert a peer.
     * @param[in]  candidate Candidate peer
     * @param[out] worst     Replaced, worst-performing peer
     * @return               Status of the attempted insertion:
     *   - EXISTS    Peer is already member of set
     *   - SUCCESS   Success
     *   - REPLACED  Success. `*replaced` is set iff `replaced != nullptr`
     *   - FULL      Set is full and insufficient time to determine worst peer
     * @exceptionsafety      Strong guarantee
     * @threadsafety         Safe
     */
    InsertStatus tryInsert(
            Peer& candidate,
            Peer* worst = nullptr) const;

    /**
     * Tries to insert a remote peer given its Internet socket address.
     * @param[in]  candidate   Candidate remote peer
     * @param[in,out] msgRcvr  Receiver of messages from the remote peer
     * @param[out] replaced    Replaced, worst-performing peer
     * @return                 Insertion status:
     *   - EXISTS    Peer is already member of set
     *   - SUCCESS   Success
     *   - REPLACED  Success. `*replaced` is set iff `replaced != nullptr`
     *   - FULL      Set is full and insufficient time to determine worst peer
     * @exceptionsafety       Strong guarantee
     * @threadsafety          Safe
     */
    InsertStatus tryInsert(
            const InetSockAddr& candidate,
            PeerMsgRcvr&        msgRcvr,
            Peer*               replaced);

    /**
     * Sends information about a product to all peers in the set.
     * @param[in] prodInfo        Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo);

    /**
     * Sends information about a product to all peers in the set except one.
     * @param[in] prodInfo        Product information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo, const Peer& except);

    /**
     * Sends information about a chunk-of-data to all peers in the set.
     * @param[in] chunkInfo       Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo);

    /**
     * Sends information about a chunk-of-data to all peers in the set except
     * one.
     * @param[in] chunkInfo       Chunk information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo, const Peer& except);

    /**
     * Increments the value of a peer.
     * @param[in] peer  Peer to have its value incremented
     */
    void incValue(Peer& peer) const;

    /**
     * Indicates if this instance is full.
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    bool isFull() const;
};

} // namespace

#endif /* PEERSET_H_ */
