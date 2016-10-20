/**
 * This file declares an interface for a peer.
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

#include <exception>
#include <memory>

namespace hycast {

class PeerMgr {
public:
    virtual ~PeerMgr() {};

    virtual void sendNotice(const ProdInfo& info) =0;
    virtual void recvNotice(const ProdInfo& info) =0;

    virtual void sendNotice(const ChunkInfo& info) =0;
    virtual void recvNotice(const ChunkInfo& info) =0;

    virtual void sendRequest(const ProdIndex& index) =0;
    virtual void recvRequest(const ProdIndex& index) =0;

    virtual void sendRequest(const ChunkInfo& info) =0;
    virtual void recvRequest(const ChunkInfo& info) =0;

    virtual void sendData(const ActualChunk& chunk) =0;
    /*
     * For an unknown reason, the compiler complains if the `recvData` parameter
     * is a `LatentChunk&` and not a `LatentChunk`. This is acceptable, however,
     * because `LatentChunk` uses the pImpl idiom. See
     * `PeerConnectionImpl::runReceiver`.
     */
    virtual void recvData(LatentChunk chunk) =0;

    virtual void recvEof() = 0;
    virtual void recvException(const std::exception& e) =0;
};

} // namespace

#endif /* PEERMGR_H_ */
