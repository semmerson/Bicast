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

#include "PeerConnection.h"
#include "PeerConnectionImpl.h"

namespace hycast {

PeerConnection::PeerConnection(
        Peer&    peer,
        Socket&  sock,
        unsigned version)
    : pImpl(new PeerConnectionImpl(peer, sock, version))
{
}

void PeerConnection::sendNotice(const ProdInfo& prodInfo)
{
    pImpl->sendProdInfo(prodInfo);
}

void PeerConnection::sendNotice(const ChunkInfo& chunkInfo)
{
    pImpl->sendChunkInfo(chunkInfo);
}

void PeerConnection::sendRequest(const ProdIndex& prodIndex)
{
    pImpl->sendProdRequest(prodIndex);
}

void PeerConnection::sendRequest(const ChunkInfo& info)
{
    pImpl->sendRequest(info);
}

void PeerConnection::sendData(const ActualChunk& chunk)
{
    pImpl->sendData(chunk);
}

} // namespace
