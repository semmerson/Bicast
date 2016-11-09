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
        Socket&  sock)
    : pImpl(new PeerImpl(peerMgr, sock, *this))
{
}

void Peer::runReceiver()
{
    pImpl->runReceiver();
}

void Peer::sendNotice(const ProdInfo& prodInfo) const
{
    pImpl->sendProdInfo(prodInfo);
}

void Peer::sendNotice(const ChunkInfo& chunkInfo) const
{
    pImpl->sendChunkInfo(chunkInfo);
}

void Peer::sendRequest(const ProdIndex& prodIndex) const
{
    pImpl->sendProdRequest(prodIndex);
}

void Peer::sendRequest(const ChunkInfo& info) const
{
    pImpl->sendRequest(info);
}

void Peer::sendData(const ActualChunk& chunk) const
{
    pImpl->sendData(chunk);
}

bool Peer::areEqual(const Peer& peer1, const Peer& peer2)
{
    return peer1.pImpl->equals(*peer2.pImpl.get());
}

size_t Peer::hash(const Peer& peer)
{
    return peer.pImpl->hash();
}

bool Peer::operator ==(const Peer& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

bool Peer::operator <(const Peer& that) const noexcept
{
    return *pImpl.get() < *that.pImpl.get();
}

unsigned Peer::getNumStreams()
{
    return PeerImpl::getNumStreams();
}

} // namespace
