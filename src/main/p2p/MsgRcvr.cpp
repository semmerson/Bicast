/**
 * This file implements the interface for an object that receives messages from
 * a remote peer.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: MsgRcvr.cpp
 * @author: Steven R. Emmerson
 */

#include "MsgRcvr.h"
#include "MsgRcvrImpl.h"

namespace hycast {

MsgRcvr::MsgRcvr(MsgRcvrImpl* impl)
    : pImpl{impl}
{}

void MsgRcvr::recvNotice(const ProdInfo& info, Peer& peer) const
{
    pImpl->recvNotice(info, peer);
}

void MsgRcvr::recvNotice(const ChunkInfo& info, Peer& peer) const
{
    pImpl->recvNotice(info, peer);
}

void MsgRcvr::recvRequest(const ProdIndex& index, Peer& peer) const
{
    pImpl->recvRequest(index, peer);
}

void MsgRcvr::recvRequest(const ChunkInfo& info, Peer& peer) const
{
    pImpl->recvRequest(info, peer);
}

void MsgRcvr::recvData(LatentChunk chunk, Peer& peer) const
{
    pImpl->recvData(chunk, peer);
}

} // namespace
