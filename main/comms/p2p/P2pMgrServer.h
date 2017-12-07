/**
 * This file declares the interface for the higher-level component used by a
 * manager of a set of active peers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: P2pMgrServer.h
 * @author: Steven R. Emmerson
 */

#ifndef P2PMGRSERVER_H_
#define P2PMGRSERVER_H_

#include "P2pServer.h"
#include "BackloggerFactory.h"
#include "P2pContentRcvr.h"

namespace hycast {

class P2pMgrServer : public BackloggerFactory
                   , public P2pServer
                   , public P2pContentRcvr
{
public:
    virtual ~P2pMgrServer() =default;
    /**
     * Returns the ID of the earliest missing chunk-of-data.
     * @return ID of earliest missing data-chunk
     */
    virtual ChunkId getEarliestMissingChunkId() =0;
};

} // namespace

#endif /* P2PMGRSERVER_H_ */
