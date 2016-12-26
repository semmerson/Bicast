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

#ifndef MAIN_P2P_P2PMGRIMPL_H_
#define MAIN_P2P_P2PMGRIMPL_H_

#include "Executor.h"
#include "Future.h"
#include "InetSockAddr.h"
#include "MsgRcvr.h"
#include "PeerSet.h"
#include "PotentialPeers.h"

#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

namespace hycast {

class P2pMgrImpl final {
    InetSockAddr    serverSockAddr; /// Internet address of peer-server
    MsgRcvr&        msgRcvr;        /// Object to receive incoming messages
    PotentialPeers* potentialPeers; /// Source of potential peers
    PeerSet         peerSet;        /// Set of active peers
    Executor<void>  executor;

    /**
     * Runs the server for connections from remote peers. Doesn't return unless
     * an exception is thrown.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void runServer();
    /**
     * Performs peer-replacement, which replaces the worst-performing peer if
     * the set of active peers is full, has been unchanged for the required
     * amount of time, and a potential replacement peer is available. Doesn't
     * return unless an exception is thrown.
     */
    void runReplacer();
public:
    /**
     * Constructs.
     * @param[in]     serverSockAddr  Socket address to be used by the server
     *                                that remote peers connect to
     * @param[in]     peerCount       Canonical number of active peers
     * @param[in]     potentialPeers  Source of potential replacement peers or
     *                                `nullptr`, in which case no replacement is
     *                                performed
     * @param[in]     stasisDuration  Time interval, in seconds, over which the
     *                                set of active peers must be unchanged
     *                                before the worst performing peer may be
     *                                replaced
     * @param[in,out] msgRcvr         Receiver of messages from remote peers
     */
    P2pMgrImpl(
            InetSockAddr&   serverSockAddr,
            unsigned        peerCount,
            PotentialPeers* potentialPeers,
            unsigned        stasisDuration,
            MsgRcvr&        msgRcvr);
    /**
     * Runs this instance. Starts receiving connection requests from remote
     * peers. Adds peers to the set of active peers. Replaces the worst
     * performing peer when appropriate. Doesn't return unless an exception is
     * thrown. The thread on which this function executes may be safely
     * cancelled.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void run();
};

} // namespace

#endif /* MAIN_P2P_P2PMGRIMPL_H_ */
