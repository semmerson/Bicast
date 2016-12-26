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

PeerSet::PeerSet(
        const unsigned maxPeers,
        const unsigned stasisDuration)
    : pImpl(new PeerSetImpl(maxPeers, stasisDuration))
{}

PeerSet::InsertStatus PeerSet::tryInsert(
        Peer& candidate,
        Peer* replaced) const
{
    PeerSetImpl::InsertStatus status = pImpl->tryInsert(candidate, replaced);
    return status == PeerSetImpl::InsertStatus::FAILURE
            ? PeerSet::InsertStatus::FAILURE
            : status == PeerSetImpl::InsertStatus::SUCCESS
                ? PeerSet::InsertStatus::SUCCESS
                : PeerSet::InsertStatus::REPLACED;
}

void PeerSet::sendNotice(const ProdInfo& prodInfo) const
{
    pImpl->sendNotice(prodInfo);
}

void PeerSet::sendNotice(const ChunkInfo& chunkInfo) const
{
    pImpl->sendNotice(chunkInfo);
}

void PeerSet::incValue(Peer& peer) const
{
    pImpl->incValue(peer);
}

} // namespace
