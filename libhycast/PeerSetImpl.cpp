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
    for (const auto& peer : set)
        peer.sendNotice(prodInfo);
}

void PeerSetImpl::sendNotice(const ChunkInfo& chunkInfo)
{
    for (const auto& peer : set)
        peer.sendNotice(chunkInfo);
}

} // namespace
