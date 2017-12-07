/**
 * This file declares the interface for a component that accepts content from
 * the peer-to-peer network.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: P2pContentRcvr.h
 * @author: Steven R. Emmerson
 */

#ifndef P2PCONTENTRCVR_H_
#define P2PCONTENTRCVR_H_

#include "RecvStatus.h"
#include "Chunk.h"
#include "ProdInfo.h"

namespace hycast {

class P2pContentRcvr
{
public:
    virtual ~P2pContentRcvr() =default;

    /**
     * Receives information about a product.
     * @param[in] info      Product information
     * @param[in] peerAddr  Address of remote peer
     */
    virtual RecvStatus receive(
            const ProdInfo&     info,
            const InetSockAddr& peerAddr) =0;

    /**
     * Receives a chunk-of-data.
     * @param[in] chunk     Data-chunk
     * @param[in] peerAddr  Address of remote peer
     */
    virtual RecvStatus receive(
            LatentChunk&        chunk,
            const InetSockAddr& peerAddr) =0;
};

} // namespace

#endif /* P2PCONTENTRCVR_H_ */
