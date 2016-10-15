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

#include <memory>

namespace hycast {

class Peer {
public:
    ~Peer() =0;

    void sendProdInfo(std::shared_ptr<ProdInfo>& info) =0;
    void recvProdInfo(std::shared_ptr<ProdInfo>& info) =0;

    void sendChunkInfo(std::shared_ptr<ChunkInfo>& info) =0;
    void recvChunkInfo(std::shared_ptr<ChunkInfo>& info) =0;

    void sendProdRequest(ProdIndex& index) =0;
    void recvProdRequest(ProdIndex& index) =0;

    void sendChunkRequest(ChunkInfo& info) =0;
    void recvChunkRequest(ChunkInfo& info) =0;

    void sendChunk(ActualChunk& chunk) =0;
    void recvChunk(LatentChunk& chunk) =0;
};

} // namespace

#endif /* PEER_H_ */
