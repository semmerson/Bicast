/**
 * This file declares the interface for a component that creates backloggers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: BackloggerFactory.h
 * @author: Steven R. Emmerson
 */

#ifndef BACKLOGGERFACTORY_H_
#define BACKLOGGERFACTORY_H_

#include "Backlogger.h"
#include "Chunk.h"
#include "Peer.h"

namespace hycast {

class BackloggerFactory
{
public:
    virtual ~BackloggerFactory() =default;

    /**
     * Returns an object that, when executed, will send the requested backlog of
     * data-chunk notices to a remote peer.
     * @param[in] earliest  ID of earliest missing data-chunk
     * @param[in] peer      Remote peer that requested backlog
     * @return              Backlog object
     */
    virtual Backlogger getBacklogger(
            const ChunkId& earliest,
            PeerMsgSndr&          peer) =0;
};

} // namespace

#endif /* BACKLOGGERFACTORY_H_ */
