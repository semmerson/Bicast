/**
 * This file implements an object channel for chunks-of-data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkChannel.cpp
 * @author: Steven R. Emmerson
 */

#include "ChunkChannel.h"
#include "ChunkChannelImpl.h"

namespace hycast {

ChunkChannel::ChunkChannel(
        Socket&        sock,
        const unsigned streamId,
        const unsigned version)
    : pImpl(new ChunkChannelImpl(sock, streamId, version))
{
}

Socket& ChunkChannel::getSocket() const
{
    return pImpl->getSocket();
}

unsigned ChunkChannel::getStreamId() const
{
    return pImpl->getStreamId();
}

void ChunkChannel::send(const ActualChunk& chunk) const
{
    pImpl->send(chunk);
}

std::shared_ptr<LatentChunk> ChunkChannel::recv()
{
    return pImpl->recv();
}

size_t ChunkChannel::getSize() const
{
    return pImpl->getSize();
}

} // namespace
