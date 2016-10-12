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

#include <cstddef>
#include <sys/uio.h>

namespace hycast {

ChunkChannelImpl::ChunkChannelImpl(
        Socket&        sock,
        const unsigned streamId,
        const unsigned version)
    : sock(sock),
      streamId(streamId),
      version(version)
{
}

void ChunkChannelImpl::send(const ActualChunk& chunk)
{
    chunk.serialize(sock, streamId, version);
}

std::shared_ptr<LatentChunk> ChunkChannelImpl::recv()
{
    return std::shared_ptr<LatentChunk>(new LatentChunk(sock, version));
}

} // namespace
