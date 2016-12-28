/**
 * This file implements the implementation of a manager of peer-to-peer
 * connections, which is responsible for processing incoming connection
 * requests, creating peers, and replacing peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: P2PMgrImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "logging.h"
#include "P2pMgrImpl.h"
#include "ServerSocket.h"

#include <deque>
#include <exception>
#include <future>
#include <pthread.h>
#include <stdexcept>
#include <unistd.h>

namespace hycast {

P2pMgrImpl::P2pMgrImpl(
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

void P2pMgrImpl::runServer()
{
    throw std::logic_error("Not implemented yet");
}

void P2pMgrImpl::runReplacer()
{
    throw std::logic_error("Not implemented yet");
}

void P2pMgrImpl::run()
{
    completer.submit([=]{ runServer(); });
    if (peerSource)
        completer.submit([=]{ runReplacer(); });
    auto future = completer.get();
    if (!future.wasCancelled())
        future.getResult(); // might throw exception
}

} // namespace
