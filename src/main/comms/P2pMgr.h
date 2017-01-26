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

#ifndef MAIN_COMMS_P2PMGR_H_
#define MAIN_COMMS_P2PMGR_H_

#include "InetSockAddr.h"
#include <memory>

#include "../comms/MsgRcvr.h"
#include "../comms/Notifier.h"
#include "../comms/PeerSource.h"

namespace hycast {

class P2pMgrImpl; /// Forward declaration

class P2pMgr final : public Notifier
{
    std::shared_ptr<P2pMgrImpl> pImpl; /// `pImpl` idiom
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
    P2pMgr(
            InetSockAddr&   serverSockAddr,
            unsigned        peerCount,
            PeerSource*     peerSource,
            unsigned        stasisDuration,
            MsgRcvr&        msgRcvr);
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
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] chunkInfo       Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo) const;
};

} // namespace

#endif /* MAIN_COMMS_P2PMGR_H_ */
