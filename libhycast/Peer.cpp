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
        Socket&  sock)
    : pImpl(new PeerImpl(sock))
{
}

unsigned Peer::getNumStreams()
{
    return PeerImpl::getNumStreams();
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

MsgType Peer::getMsgType()
{
    return pImpl->getMsgType();
}

ProdInfo Peer::getProdNotice()
{
    return pImpl->getProdNotice();
}

ChunkInfo Peer::getChunkNotice()
{
    return pImpl->getChunkNotice();
}

ProdIndex Peer::getProdRequest()
{
    return pImpl->getProdRequest();
}

ChunkInfo Peer::getChunkRequest()
{
    return pImpl->getChunkRequest();
}

LatentChunk Peer::getChunk()
{
    return pImpl->getChunk();
}

} // namespace
