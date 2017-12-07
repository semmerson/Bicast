/**
 * This file declares the interface for the higher-level component used by a set
 * of peers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSetServer.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERSETSERVER_H_
#define PEERSETSERVER_H_

#include "P2pMgrServer.h"

namespace hycast {

class PeerSetServer : public P2pMgrServer
{
public:
    virtual ~PeerSetServer() =default;

    /**
     * Handles a peer that stopped (for whatever reason).
     * @param[in] peerAddr  Address of the remote peer
     */
    virtual void peerStopped(const InetSockAddr& peerAddr) =0;
};

} // namespace

#endif /* PEERSETSERVER_H_ */
