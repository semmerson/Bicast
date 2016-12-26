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

#include "P2pMgr.h"
#include "P2pMgrImpl.h"

namespace hycast {

hycast::P2pMgr::P2pMgr(
        InetSockAddr&   serverSockAddr,
        unsigned        peerCount,
        PotentialPeers* potentialPeers,
        unsigned        stasisDuration,
        MsgRcvr&        msgRcvr)
    : pImpl{new P2pMgrImpl(serverSockAddr, peerCount, potentialPeers,
            stasisDuration, msgRcvr)}
{}

void P2pMgr::run()
{
    pImpl->run();
}

} // namespace
