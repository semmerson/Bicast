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

#include "Completer.h"
#include "Future.h"
#include "InetSockAddr.h"
#include "MsgRcvr.h"
#include "Notifier.h"
#include "P2pMgr.h"
#include "PeerSet.h"
#include "PeerSource.h"
#include "ServerSocket.h"

#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

namespace hycast {

class P2pMgrImpl final : public Notifier
{
    InetSockAddr     serverSockAddr; /// Internet address of peer-server
    MsgRcvr&         msgRcvr;        /// Object to receive incoming messages
    PeerSource*      peerSource;     /// Source of potential peers
    PeerSet          peerSet;        /// Set of active peers
    Completer<void>  completer;      /// Asynchronous task completion service

    /**
     * Runs the server for connections from remote peers. Doesn't return unless
     * an exception is thrown.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void runServer()
    {
        //throw std::logic_error("Not implemented yet");
        auto serverSock = ServerSocket(serverSockAddr, Peer::getNumStreams());
        for (;;) {
            auto sock = serverSock.accept();
            auto peer = Peer(msgRcvr, sock);
            peerSet.tryInsert(peer, nullptr);
        }
    }
    /**
     * Performs peer-replacement, which replaces the worst-performing peer if
     * the set of active peers is full, has been unchanged for the required
     * amount of time, and a potential replacement peer is available. Doesn't
     * return unless an exception is thrown.
     */
    void runReplacer()
    {
        throw std::logic_error("Not implemented yet");
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
        , peerSet{peerCount, stasisDuration}
        , completer{}
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
        completer.submit([=]{ runServer(); });
        if (peerSource)
            completer.submit([=]{ runReplacer(); });
        auto future = completer.get();
        completer.shutdownNow();
        completer.awaitTermination();
        if (!future.wasCancelled())
            future.getResult(); // might throw exception
    }
    /**
     * Sends information about a product to the remote peers.
     * @param[in] prodInfo        Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo) const
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
    void sendNotice(const ChunkInfo& chunkInfo) const
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

void P2pMgr::sendNotice(const ProdInfo& prodInfo) const
{
    pImpl->sendNotice(prodInfo);
}

void P2pMgr::sendNotice(const ChunkInfo& chunkInfo) const
{
    pImpl->sendNotice(chunkInfo);
}

} // namespace
