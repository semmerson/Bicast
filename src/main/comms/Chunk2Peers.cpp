/**
 * This file maps from data-chunk specifications to lists of peers that have the
 * data-chunk.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Chunk2Peers.cpp
 * @author: Steven R. Emmerson
 */

#include "../comms/Chunk2Peers.h"

namespace hycast {

Chunk2Peers::Chunk2Peers()
    : map{16}
{}

void Chunk2Peers::add(
        const ChunkInfo& info,
        Peer&            peer)
{
    map[info].push_back(peer);
}

Chunk2Peers::PeerBounds Chunk2Peers::getPeers(const ChunkInfo& info) const
{
    Map::const_iterator listIter{map.find(info)};
    if (listIter == map.end()) {
        Map::mapped_type::const_iterator emptyIter{};
        return PeerBounds{emptyIter, emptyIter};
    }
    return PeerBounds{listIter->second.begin(), listIter->second.end()};
}

void Chunk2Peers::remove(const ChunkInfo& info)
{
    map.erase(info);
}

void Chunk2Peers::remove(
        const ChunkInfo& info,
        const Peer&      peer)
{
    Map::iterator listIter{map.find(info)};
    if (listIter != map.end())
        listIter->second.remove(peer);
}

} // namespace
