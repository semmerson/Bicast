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

#ifndef PEER_H_
#define PEER_H_

#include "Chunk.h"
#include "ChunkInfo.h"
#include "ProdIndex.h"
#include "ProdInfo.h"

#include <exception>
#include <memory>

namespace hycast {

class Peer {
public:
    virtual ~Peer() {};

    virtual void sendProdInfo(const ProdInfo& info) =0;
    virtual void recvProdInfo(const ProdInfo& info) =0;

    virtual void sendChunkInfo(const ChunkInfo& info) =0;
    virtual void recvChunkInfo(const ChunkInfo& info) =0;

    virtual void sendProdRequest(const ProdIndex& index) =0;
    virtual void recvProdRequest(const ProdIndex& index) =0;

    virtual void sendChunkRequest(const ChunkInfo& info) =0;
    virtual void recvChunkRequest(const ChunkInfo& info) =0;

    virtual void sendChunk(const ActualChunk& chunk) =0;
    virtual void recvChunk(LatentChunk& chunk) =0;

    virtual void recvEof() = 0;
    virtual void recvException(const std::exception& e) =0;
};

} // namespace

#endif /* PEER_H_ */
