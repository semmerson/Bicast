/**
 * This file implements two types of chunks of data: 1) a latent chunk that must
 * be read from an object channel; and 2) a reified chunk with a pointer to its
 * data. The two types are in the same file to support keeping their
 * serialization and de-serialization methods consistent.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Chunk.cpp
 * @author: Steven R. Emmerson
 */

#include "Chunk.h"
#include "ChunkImpl.h"

namespace hycast {

ActualChunk::ActualChunk()
    : pImpl(new ActualChunkImpl())
{
}

ActualChunk::ActualChunk(
        const ChunkInfo& info,
        const void*      data,
        const ChunkSize  size)
    : pImpl(new ActualChunkImpl(info, data, size))
{
}

const ChunkInfo& ActualChunk::getInfo() const noexcept
{
    return pImpl->getInfo();
}

ChunkSize ActualChunk::getSize() const noexcept
{
    return pImpl->getSize();
}

const void* ActualChunk::getData() const noexcept
{
    return pImpl->getData();
}

void ActualChunk::serialize(
        Socket&        sock,
        const unsigned streamId,
        const unsigned version) const
{
    pImpl->serialize(sock, streamId, version);
}

LatentChunk::LatentChunk(
        Socket&        sock,
        const unsigned version)
    : pImpl(new LatentChunkImpl(sock, version))
{
}

const ChunkInfo& LatentChunk::getInfo() const noexcept
{
    return pImpl->getInfo();
}

ChunkSize LatentChunk::getSize() const noexcept
{
    return pImpl->getSize();
}

void LatentChunk::drainData(void* data)
{
    pImpl->drainData(data);
}

} // namespace
