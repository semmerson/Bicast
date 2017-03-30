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
#include "config.h"

#include "Completer.h"
#include "error.h"
#include "Future.h"
#include "InetSockAddr.h"
#include "MsgRcvr.h"
#include "Notifier.h"
#include "P2pMgr.h"
#include "PeerSet.h"
#include "PeerSource.h"
#include "SrvrSctpSock.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <set>
#include <thread>

namespace hycast {

/**
 * Class that implements a manager of peer-to-peer (P2P) connections.
 *
 * For reliability of data delivery, the following (overly cautious but simple)
 * protocol is implemented in order to ensure that every node in the P2P overlay
 * network has a path through the network from it to the source of the
 * data-products:
 *
 *   - An initiated peer is one that the node creates from a connection that it
 *     initiates (as opposed to a connection that it accepts).
 *   - The node accepts connections only if it has at least one initiated peer.
 *   - The node closes all connections when it has no initiated peers.
 */
class P2pMgr::Impl final : public Notifier
{
    InetSockAddr            serverSockAddr; /// Internet address of peer-server
    PeerMsgRcvr&            msgRcvr;        /// Object to receive incoming messages
    PeerSource*             peerSource;     /// Source of potential peers
    PeerSet                 peerSet;        /// Set of active peers
    Completer<void>         completer;      /// Asynchronous task completion service
    /// Concurrent access variables for peer-termination:
    std::mutex              termMutex;
    std::condition_variable termCond;
    /// Concurrent access mutex for initiated peers:
    std::mutex              initMutex;
    /// Duration to wait before trying to replace the worst-performing peer.
    std::chrono::seconds    waitDuration;
    /// Remote socket address of initiated peers
    std::set<InetSockAddr>  initiated;

    typedef std::lock_guard<decltype(termMutex)>  LockGuard;
    typedef std::unique_lock<decltype(termMutex)> UniqueLock;

    /**
     * Accepts a connection from a remote peer. Creates a corresponding local
     * peer and tries to add it to the set of active peers. Intended to run on
     * its own thread.
     * @param[in] sock   Incoming connection
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void accept(SctpSock sock)
    {
        try {
            // Blocks exchanging protocol version. Hence, separate thread
            auto peer = Peer(msgRcvr, sock);
            peerSet.tryInsert(peer, nullptr);
        }
        catch (const std::exception& e) {
            log_what(e); // Because end of thread
        }
    }

    /**
     * Runs the server for connections from remote peers. Creates a
     * corresponding local peer and attempts to add it to the set of active
     * peers if and only if at least one initiated peer exists. Doesn't return
     * unless an exception is thrown. Intended to be run on a separate thread.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void runServer()
    {
        try {
            auto serverSock =
                    SrvrSctpSock(serverSockAddr, Peer::getNumStreams());
            for (;;) {
                SctpSock sock = serverSock.accept(); // Blocks listening
                LockGuard lock(initMutex);
                if (initiated.size()) {
                    std::thread([=]{accept(sock);}).detach();
                }
                else {
                    sock.close();
                }
            }
        }
        catch (const std::exception& e) {
            log_what(e); // Because end of thread
        }
    }

    /**
     * Attempts to adds peers to the set of active peers when
     *   - Initially called; or
     *   - `waitDuration` time passes since the last addition attempt; or
     *   - The set of active peers becomes not full,
     * whichever occurs first. Doesn't return unless an exception is thrown.
     * Intended to run on its own thread.
     */
    void runPeerAdder()
    {
        throw LogicError(__FILE__, __LINE__, "Not implemented yet");
        try {
            for (;;) {
                 auto pair = peerSource->getPeers();
                 for (auto iter = pair.first; iter != pair.second; ++iter) {
                     auto status = peerSet.tryInsert(*iter, msgRcvr, nullptr);
                     if (status == PeerSet::FULL)
                         break;
                     if (status == PeerSet::SUCCESS || status == PeerSet::REPLACED) {
                         LockGuard lock(initMutex);
                         initiated.insert(*iter);
                     }
                 }
                 UniqueLock lock(termMutex);
                 auto when = std::chrono::steady_clock::now() + waitDuration;
                 while (peerSet.isFull())
                     termCond.wait_until(lock, when);
            }
        }
        catch (const std::exception& e) {
            log_what(e); // Because end of thread
        }
    }

    /***
     * Handles termination of a peer. Called by the active peer-set when one of
     * its peers terminates. Removes the peer from the set of initiated peers,
     * if appropriate, and prematurely wakes-up the adder of initiated peer.
     */
    void peerTerminated(Peer& peer)
    {
        {
            LockGuard lock(initMutex);
            initiated.erase(peer.getRemoteAddr());
        }
        {
            LockGuard lock(termMutex);
            termCond.notify_one();
        }
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
    Impl(
            InetSockAddr&   serverSockAddr,
            unsigned        peerCount,
            PeerSource*     peerSource,
            unsigned        stasisDuration,
            PeerMsgRcvr&    msgRcvr)
        : serverSockAddr{serverSockAddr}
        , msgRcvr(msgRcvr)
        , peerSource{peerSource}
        , peerSet{[=](Peer& p){peerTerminated(p);}, peerCount, stasisDuration}
        , completer{}
        , termMutex{}
        , termCond{}
        , initMutex{}
        , waitDuration{stasisDuration+1}
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
     * Sends information about a product to the remote peers except for one.
     * @param[in] prodInfo        Product information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo, const Peer& except)
    {
        peerSet.sendNotice(prodInfo, except);
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

    /**
     * Sends information about a chunk-of-data to the remote peers except for
     * one.
     * @param[in] chunkInfo       Chunk information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo, const Peer& except)
    {
        peerSet.sendNotice(chunkInfo, except);
    }
};

hycast::P2pMgr::P2pMgr(
        InetSockAddr&   serverSockAddr,
        unsigned        peerCount,
        PeerSource*     potentialPeers,
        unsigned        stasisDuration,
        PeerMsgRcvr&    msgRcvr)
    : pImpl{new Impl(serverSockAddr, peerCount, potentialPeers, stasisDuration,
            msgRcvr)}
{}

void P2pMgr::operator()()
{
    pImpl->operator()();
}

void P2pMgr::sendNotice(const ProdInfo& prodInfo)
{
    pImpl->sendNotice(prodInfo);
}

void P2pMgr::sendNotice(const ProdInfo& prodInfo, const Peer& except)
{
    pImpl->sendNotice(prodInfo, except);
}

void P2pMgr::sendNotice(const ChunkInfo& chunkInfo)
{
    pImpl->sendNotice(chunkInfo);
}

void P2pMgr::sendNotice(const ChunkInfo& chunkInfo, const Peer& except)
{
    pImpl->sendNotice(chunkInfo, except);
}

} // namespace
