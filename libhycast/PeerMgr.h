/**
 * This file declares an interface for a peer manager.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Peer.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERMGR_H_
#define PEERMGR_H_

#include "Chunk.h"
#include "ChunkInfo.h"
#include "ProdIndex.h"
#include "ProdInfo.h"

namespace hycast {

class PeerMgr {
public:
    virtual ~PeerMgr() {};

    virtual void recvNotice(const ProdInfo& info) =0;

    virtual void recvNotice(const ChunkInfo& info) =0;

    virtual void recvRequest(const ProdIndex& index) =0;

    virtual void recvRequest(const ChunkInfo& info) =0;

    /*
     * For an unknown reason, the compiler complains if the `recvData` parameter
     * is a `LatentChunk&` and not a `LatentChunk`. This is acceptable, however,
     * because `LatentChunk` uses the pImpl idiom. See
     * `PeerConnectionImpl::runReceiver`.
     */
    virtual void recvData(LatentChunk chunk) =0;
};

} // namespace

#endif /* PEERMGR_H_ */
