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

#include "Chunk2Peers.h"

namespace hycast {

void Chunk2Peers::add(
        const ChunkInfo& info,
        Peer&            peer)
{
    map[info].push_back(peer);
}

Peer* Chunk2Peers::getFrontPeer(const ChunkInfo& info)
{
    decltype(map)::iterator iter{map.find(info)};
    return iter == map.end()
            ? nullptr
            : &iter->second.front(); // Safe because empty lists don't exist
}

void Chunk2Peers::remove(const ChunkInfo& info)
{
    map.erase(info);
}

void Chunk2Peers::remove(
        const ChunkInfo& info,
        const Peer&      peer)
{
    decltype(map)::iterator iter{map.find(info)};
    if (iter != map.end()) {
        std::list<Peer> list = iter->second;
        list.remove(peer);
        if (list.empty())
            map.erase(info); // Empty lists must not exist
    }
}

} // namespace
