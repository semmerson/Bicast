/**
 * This file defines the implementation of a set of peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSetImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "PeerSetImpl.h"

#include <algorithm>

namespace hycast {

void PeerSetImpl::sendNotice(const ProdInfo& prodInfo)
{
    std::for_each(set.begin(), set.end(),
            [&prodInfo](const Peer& peer) {peer.sendNotice(prodInfo);});
}

void PeerSetImpl::sendNotice(const ChunkInfo& chunkInfo)
{
    std::for_each(set.begin(), set.end(),
            [&chunkInfo](const Peer& peer) {peer.sendNotice(chunkInfo);});
}

} // namespace
