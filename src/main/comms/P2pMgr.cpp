/**
 * This file implements a manager of peer-to-peer connections, which is
 * responsible for processing incoming connection requests, creating peers, and
 * replacing peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: P2pMgr.cpp
 * @author: Steven R. Emmerson
 */

#include <comms/MsgRcvr.h>
#include <comms/Notifier.h>
#include <comms/P2pMgr.h>
#include <comms/PeerSet.h>
#include <comms/PeerSource.h>
#include "Completer.h"
#include "Future.h"
#include "InetSockAddr.h"
#include "SrvrSctpSock.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <thread>

namespace hycast {

class P2pMgrImpl final : public Notifier
{
    InetSockAddr            serverSockAddr; /// Internet address of peer-server
    MsgRcvr&                msgRcvr;        /// Object to receive incoming messages
    PeerSource*             peerSource;     /// Source of potential peers
    PeerSet                 peerSet;        /// Set of active peers
    Completer<void>         completer;      /// Asynchronous task completion service
    /// Synchronization variables:
    std::mutex              mutex;
    std::condition_variable cond;
    /// Duration to wait before trying to replace the worst-performing peer.
    std::chrono::seconds    waitDuration;
    bool                    addPeers; /// Predicate for premature wake-up

    /**
     * Runs the server for connections from remote peers. Doesn't return unless
     * an exception is thrown.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void runServer()
    {
        auto serverSock = SrvrSctpSock(serverSockAddr, Peer::getNumStreams());
        for (;;) {
            auto sock = serverSock.accept();
            auto peer = Peer(msgRcvr, sock);
            peerSet.tryInsert(peer, nullptr);
        }
    }
    /**
     * Attempts to adds peers to the set of active peers when
     *   - Initially called; and
     *   - `waitDuration` time passes since the last addition attempt or `cond`
     *     is notified and `addPeers` is true, whichever occurs first.
     * Doesn't return unless an exception is thrown.
     */
    void runPeerAdder()
    {
        throw std::logic_error("Not implemented yet");
        for (;;) {
             auto pair = peerSource->getPeers();
             for (auto iter = pair.first; iter != pair.second; ++iter) {
                 auto status = peerSet.tryInsert(*iter, msgRcvr, nullptr);
                 if (status == PeerSet::FULL)
                     break;
             }
             std::unique_lock<decltype(mutex)> lock(mutex);
             cond.wait_for(lock, waitDuration, [&]{return addPeers;});
        }
    }
    /***
     * Prematurely wakes-up the peer-adder. Called by the active peer-set when a
     * peer terminates.
     */
    void peerTerminated()
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        addPeers = true;
        cond.notify_one();
    }

public:
    /**
     * Constructs.
     * @param[in]     serverSockAddr  Socket address to be used by the server
     *                                that remote peers connect to
     * @param[in]     peerCount       Canonical number of active peers
     * @param[in]     peerSource      Source of potential replacement peers or
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
            PeerSource*     peerSource,
            unsigned        stasisDuration,
            MsgRcvr&        msgRcvr)
        : serverSockAddr{serverSockAddr}
        , msgRcvr(msgRcvr)
        , peerSource{peerSource}
        , peerSet{[=]{peerTerminated();}, peerCount, stasisDuration}
        , completer{}
        , mutex{}
        , cond{}
        , waitDuration{stasisDuration+1}
        , addPeers{false}
    {}

    /**
     * Runs this instance. Starts receiving connection requests from remote
     * peers. Adds peers to the set of active peers. Replaces the worst
     * performing peer when appropriate. Doesn't return unless an exception is
     * thrown. The thread on which this function executes may be safely
     * cancelled.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void operator()()
    {
        completer.submit([&]{ runServer(); });
        if (peerSource)
            completer.submit([&]{ runPeerAdder(); });
        auto future = completer.get(); // Blocks until exception thrown
        // Futures never cancelled => future.wasCancelled() is unnecessary
        future.getResult(); // might throw exception
    }

    /**
     * Sends information about a product to the remote peers.
     * @param[in] prodInfo        Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo)
    {
        peerSet.sendNotice(prodInfo);
    }

    /**
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] chunkInfo       Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo)
    {
        peerSet.sendNotice(chunkInfo);
    }
};

hycast::P2pMgr::P2pMgr(
        InetSockAddr&   serverSockAddr,
        unsigned        peerCount,
        PeerSource* potentialPeers,
        unsigned        stasisDuration,
        MsgRcvr&        msgRcvr)
    : pImpl{new P2pMgrImpl(serverSockAddr, peerCount, potentialPeers,
            stasisDuration, msgRcvr)}
{}

void P2pMgr::operator()()
{
    pImpl->operator()();
}

void P2pMgr::sendNotice(const ProdInfo& prodInfo)
{
    pImpl->sendNotice(prodInfo);
}

void P2pMgr::sendNotice(const ChunkInfo& chunkInfo)
{
    pImpl->sendNotice(chunkInfo);
}

} // namespace
