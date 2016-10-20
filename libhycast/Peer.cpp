/**
 * This file implements a connection between peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnectionImpl.cpp
 * @author: Steven R. Emmerson
 */

#include <Peer.h>
#include <PeerImpl.h>

namespace hycast {

Peer::Peer(
        PeerMgr& peerMgr,
        Socket&  sock,
        unsigned version)
    : pImpl(new PeerImpl(peerMgr, sock, version))
{
}

void Peer::sendNotice(const ProdInfo& prodInfo)
{
    pImpl->sendProdInfo(prodInfo);
}

void Peer::sendNotice(const ChunkInfo& chunkInfo)
{
    pImpl->sendChunkInfo(chunkInfo);
}

void Peer::sendRequest(const ProdIndex& prodIndex)
{
    pImpl->sendProdRequest(prodIndex);
}

void Peer::sendRequest(const ChunkInfo& info)
{
    pImpl->sendRequest(info);
}

void Peer::sendData(const ActualChunk& chunk)
{
    pImpl->sendData(chunk);
}

} // namespace
