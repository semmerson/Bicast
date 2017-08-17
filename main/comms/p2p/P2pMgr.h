/**
 * This file declares the implementation of a manager of peer-to-peer
 * connections, which is responsible for processing incoming connection
 * requests, creating peers, and replacing peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: P2pMgr.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_P2P_P2PMGR_H_
#define MAIN_P2P_P2PMGR_H_

#include "DelayQueue.h"
#include "InetSockAddr.h"
#include "MsgRcvr.h"
#include "Notifier.h"
#include "PeerSet.h"
#include "PeerSource.h"

#include <memory>

namespace hycast {

struct P2pInfo final
{
    /// Socket address to be used by the server that remote peers connect to
    InetSockAddr             serverSockAddr;

    /// Canonical number of active peers
    unsigned                 peerCount;

    /**
     * Source of potential replacement peers.
     */
    DelayQueue<InetSockAddr> peerAddrs;

    /**
     * Time interval, in seconds, over which the set of active peers must be
     * unchanged before the worst performing peer may be replaced
     */
    PeerSet::TimeUnit       stasisDuration;
};

class P2pMgr final : public Notifier
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl; /// `pImpl` idiom

public:
    /**
     * Constructs.
     * @param[in] serverSockAddr  Socket address to be used by the server that
     *                            remote peers connect to
     * @param[in] msgRcvr         Receiver of messages from remote peers
     * @param[in] maxPeers        Maximum number of active peers. Default is
     *                            `PeerSet::defaultMaxPeers`.
     * @param[in] peerAddrs       Addresses of potential peers. Default is an
     *                            empty set.
     * @param[in] stasisDuration  Time interval, in units of
     *                            `PeerSet::TimeUnit`, over which the set
     *                            of active peers must be unchanged before the
     *                            worst performing peer may be replaced. Default
     *                            is 60 seconds.
     */
    P2pMgr( const InetSockAddr&      serverSockAddr,
            PeerMsgRcvr&             msgRcvr,
            const unsigned           maxPeers = PeerSet::defaultMaxPeers,
            DelayQueue<InetSockAddr> peerAddrs = DelayQueue<InetSockAddr>(),
            const PeerSet::TimeUnit  stasisDuration = PeerSet::TimeUnit{60});

    /**
     * Constructs.
     * @param[in] p2pInfo  Information for the peer-to-peer component
     * @param[in] msgRcvr  Receiver of messages from remote peers
     */
    P2pMgr( const P2pInfo& p2pInfo,
            PeerMsgRcvr&   msgRcvr)
    	: P2pMgr(p2pInfo.serverSockAddr, msgRcvr, p2pInfo.peerCount,
    	        p2pInfo.peerAddrs, p2pInfo.stasisDuration)
    {}

    /**
     * Runs this instance. Starts receiving connection requests from remote
     * peers. Adds peers to the set of active peers. Replaces the worst
     * performing peer when appropriate. Doesn't return unless an exception is
     * thrown.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void operator()();

    /**
     * Sends information about a product to the remote peers.
     * @param[in] prodInfo        Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo) const;

    /**
     * Sends information about a product to the remote peers except for one.
     * @param[in] prodInfo        Product information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo, const Peer& except) const;

    /**
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] chunkInfo       Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo) const;

    /**
     * Sends information about a chunk-of-data to the remote peers except for
     * one.
     * @param[in] chunkInfo       Chunk information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo, const Peer& except) const;
};

} // namespace

#endif /* MAIN_P2P_P2PMGR_H_ */
