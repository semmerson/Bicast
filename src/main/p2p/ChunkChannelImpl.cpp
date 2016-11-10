/**
 * This file implements an object channel for chunks-of-data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkChannelImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "ChunkChannelImpl.h"

namespace hycast {

ChunkChannelImpl::ChunkChannelImpl(
        Socket&        sock,
        const unsigned streamId,
        const unsigned version)
    : ChannelImpl::ChannelImpl(sock, streamId, version)
{
}

void ChunkChannelImpl::send(const ActualChunk& chunk)
{
    chunk.serialize(sock, streamId, version);
}

LatentChunk ChunkChannelImpl::recv()
{
    return LatentChunk(sock, version);
}

} // namespace
