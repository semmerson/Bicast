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

#include "Peer.h"
#include "PeerImpl.h"

namespace hycast {

Peer::Peer()
    : pImpl(new PeerImpl(this))
{}

Peer::Peer(
        MsgRcvr& msgRcvr,
        Socket&  sock)
    : pImpl(new PeerImpl(this, msgRcvr, sock))
{}

void Peer::runReceiver() const
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

bool Peer::operator ==(const Peer& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

bool Peer::operator !=(const Peer& that) const noexcept
{
    return *pImpl.get() != *that.pImpl.get();
}

bool Peer::operator <(const Peer& that) const noexcept
{
    return *pImpl.get() < *that.pImpl.get();
}

uint16_t Peer::getNumStreams()
{
    return PeerImpl::getNumStreams();
}

size_t Peer::hash() const noexcept
{
    return pImpl->hash();
}

std::string Peer::to_string() const
{
    return pImpl->to_string();
}

} // namespace
