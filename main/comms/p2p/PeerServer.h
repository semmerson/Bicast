/**
 * This file declares the interface for the higher-level component used by a
 * peer.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerServer.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERSERVER_H_
#define PEERSERVER_H_

#include "P2pServer.h"

#include <memory>

namespace hycast {

class PeerServer : public P2pServer
{
public:
    virtual ~PeerServer() =default;

    /**
     * Starts sending the backlog of data-chunks. Doesn't block.
     * @param[in] earliest  Identifier of earliest data-chunk in backlog
     */
    virtual void startBacklog(const ChunkId& earliest) =0;

    /**
     * Accepts information about a product.
     * @param[in] info      Product information
     */
    virtual RecvStatus receive(const ProdInfo& info) =0;

    /**
     * Accepts a chunk-of-data.
     * @param[in] chunk     Data-chunk
     * @param[in] peerAddr  Address of remote peer
     */
    virtual RecvStatus receive(LatentChunk& chunk) =0;
};

} // namespace

#endif /* PEERSERVER_H_ */
