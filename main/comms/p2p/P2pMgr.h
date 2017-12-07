/**
 * This file declares the implementation of a manager of peer-to-peer
 * connections, which is responsible for processing incoming connection
 * requests, creating peers, processing peer-to-peer messages, and replacing
 * peers.
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

#include "Notifier.h"
#include "P2pMgrServer.h"
#include "PeerSet.h"
#include "PeerSource.h"

#include <memory>

namespace hycast {

struct P2pInfo final
{
    /// Socket address of the local peer-server
    InetSockAddr      serverSockAddr;

    /// Canonical number of active peers
    unsigned          peerCount;

    /// Source of potential remote peers.
    PeerSource&       peerSource;

    /**
     * Time interval, in seconds, over which the set of active peers must be
     * unchanged before the worst performing peer may be replaced
     */
    unsigned          stasisDuration;

    P2pInfo(const InetSockAddr&      serverSockAddr,
            const unsigned           maxPeers,
            PeerSource&              peerSource,
            const unsigned           stasisDuration)
    : serverSockAddr{serverSockAddr}
    , peerCount{maxPeers}
    , peerSource(peerSource)
    , stasisDuration{stasisDuration}
    {}
};

class P2pMgr final : public Notifier
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl; /// `pImpl` idiom

public:
    /**
     * Constructs. Upon return, the server's socket is not listening and
     * connections won't be accepted until `run()` is called.
     * @param[in] serverSockAddr  Socket address to be used by the server that
     *                            remote peers connect to
     * @param[in] p2pMgrServer    Higher-level component used by constructed
     *                            instance
     * @param[in] peerSource      Source of potential remote peers
     * @param[in] maxPeers        Maximum number of active peers. Default is
     *                            `PeerSet::defaultMaxPeers`.
     * @param[in] stasisDuration  Time interval, in units of
     *                            `PeerSet::TimeUnit`, over which the set
     *                            of active peers must be unchanged before the
     *                            worst performing peer may be replaced. Default
     *                            is 60 seconds.
     * @see `run()`
     */
    P2pMgr( const InetSockAddr& serverSockAddr,
            P2pMgrServer&       p2pMgrServer,
            PeerSource&         peerSource,
            const unsigned      maxPeers = PeerSet::defaultMaxPeers,
            const unsigned      stasisDuration = PeerSet::defaultStasisDuration);

    /**
     * Constructs.
     * @param[in] p2pInfo        Information for the peer-to-peer component
     * @param[in] p2pMgrServer   Higher-level component used by constructed
     *                           instance
     */
    P2pMgr( P2pInfo&      p2pInfo,
            P2pMgrServer& p2pMgrServer)
    	: P2pMgr(p2pInfo.serverSockAddr, p2pMgrServer, p2pInfo.peerSource,
                p2pInfo.peerCount, p2pInfo.stasisDuration)
    {}

    /**
     * Runs this instance. Starts receiving connection requests from remote
     * peers. Adds peers to the set of active peers. Replaces the worst
     * performing peer when appropriate. Doesn't return unless an exception is
     * thrown.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void run();

    /**
     * Notifies all active peers about available information on a product.
     * @param[in] prodIndex       Product index
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ProdIndex& prodIndex) const;

    /**
     * Notifies all active peers about an available chunk-of-data.
     * @param[in] chunkId         Chunk identifier
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ChunkId& chunkId) const;
};

} // namespace

#endif /* MAIN_P2P_P2PMGR_H_ */
