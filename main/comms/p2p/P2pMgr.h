/**
 * This file declares the implementation of a manager of peer-to-peer
 * connections, which is responsible for processing incoming connection
 * requests, creating peers, and replacing peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: P2PMgrImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_P2P_P2PMGR_H_
#define MAIN_P2P_P2PMGR_H_

#include "InetSockAddr.h"
#include "MsgRcvr.h"
#include "Notifier.h"
#include "PeerSource.h"

#include <memory>

namespace hycast {

struct P2pInfo final
{
    /// Socket address to be used by the server that remote peers connect to
	InetSockAddr  serverSockAddr;
	/// Canonical number of active peers
	unsigned      peerCount;
	/**
     * Source of potential replacement peers or `nullptr`, in which case no
     * replacement is performed
     */
	PeerSource*   peerSource;
	/**
     * Time interval, in seconds, over which the set of active peers must be
     * unchanged before the worst performing peer may be replaced
     */
	unsigned      stasisDuration;
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
     * @param[in] maxPeers        Maximum number of active peers
     * @param[in] peerSource      Source of potential replacement peers or
     *                            `nullptr`, in which case no replacement is
     *                            performed
     * @param[in] stasisDuration  Time interval, in seconds, over which the set
     *                            of active peers must be unchanged before the
     *                            worst performing peer may be replaced
     * @param[in] msgRcvr         Receiver of messages from remote peers
     */
    P2pMgr(
            const InetSockAddr& serverSockAddr,
            const unsigned      maxPeers,
            PeerSource*         peerSource,
            const unsigned      stasisDuration,
            PeerMsgRcvr&        msgRcvr);

    /**
     * Constructs.
     * @param[in] p2pInfo  Information for the peer-to-peer component
     * @param[in] msgRcvr  Receiver of messages from remote peers
     */
    P2pMgr( const P2pInfo& p2pInfo,
			PeerMsgRcvr&   msgRcvr)
    	: P2pMgr(p2pInfo.serverSockAddr, p2pInfo.peerCount, p2pInfo.peerSource,
    			p2pInfo.stasisDuration, msgRcvr)
    {}

    /**
     * Sets the receiver for messages from the remote peers. Must not be called
     *   - If a message-receiver was passed to the constructor; or
     *   - After `operator()()` is called.
     * @param[in] msgRcvr  Receiver of messages from remote peers
     * @throws LogicError  This instance was constructed with a message-receiver
     * @throws LogicError  `operator()()` has been called
     */
    void setMsgRcvr(PeerMsgRcvr& msgRcvr);

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
    void sendNotice(const ProdInfo& prodInfo);

    /**
     * Sends information about a product to the remote peers except for one.
     * @param[in] prodInfo        Product information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo, const Peer& except);

    /**
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] chunkInfo       Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo);

    /**
     * Sends information about a chunk-of-data to the remote peers except for
     * one.
     * @param[in] chunkInfo       Chunk information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo, const Peer& except);
};

} // namespace

#endif /* MAIN_P2P_P2PMGR_H_ */
