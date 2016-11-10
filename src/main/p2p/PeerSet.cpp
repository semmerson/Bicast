/**
 * This file implements a set of peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSet.cpp
 * @author: Steven R. Emmerson
 */

#include "PeerSet.h"
#include "PeerSetImpl.h"

namespace hycast {

PeerSet::PeerSet()
    : pImpl(new PeerSetImpl())
{}

void PeerSet::insert(Peer& peer) const
{
    pImpl->insert(peer);
}

void PeerSet::sendNotice(const ProdInfo& prodInfo) const
{
    pImpl->sendNotice(prodInfo);
}

void PeerSet::sendNotice(const ChunkInfo& chunkInfo) const
{
    pImpl->sendNotice(chunkInfo);
}

} // namespace
