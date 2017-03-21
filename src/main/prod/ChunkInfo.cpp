/**
 * This file implements information about a chunk of data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkInfo.cpp
 * @author: Steven R. Emmerson
 */

#include "ChunkInfo.h"
#include "error.h"

#include <arpa/inet.h>

namespace hycast {

// Arbitrary, but will fit in an ethernet packet
static ChunkSize canonSize = 1400;

ChunkInfo::ChunkInfo(
        const ProdIndex   prodIndex,
        const ProdSize    prodSize,
        const ChunkIndex  chunkIndex)
    : prodIndex(prodIndex)
    , prodSize(prodSize)
    , chunkOffset(chunkIndex * getCanonSize())
{
    auto numChunks = (prodSize+getCanonSize()-1)/getCanonSize();
    if (chunkIndex && chunkIndex >= numChunks)
        throw InvalidArgument(__FILE__, __LINE__,
                "Chunk-index is greater than or equal to number of chunks: "
                "index=" + std::to_string(chunkIndex) + ", numChunks=" +
                std::to_string(numChunks));
}

void ChunkInfo::setCanonSize(const ChunkSize size)
{
    if (size == 0)
        throw InvalidArgument(__FILE__, __LINE__,
                "Cannot set canonical chunk size to zero");
    canonSize = size;
}

ChunkSize ChunkInfo::getCanonSize()
{
    return canonSize;
}

ChunkSize ChunkInfo::getSize(
        const ProdSize    prodSize,
        const ChunkOffset chunkOffset)
{
    auto remaining = prodSize - chunkOffset;
    auto canonSize = getCanonSize();
    return canonSize < remaining
            ? canonSize
            : remaining;
}

ChunkInfo::ChunkInfo(
        Decoder&       decoder,
        const unsigned version)
    : ChunkInfo()
{
    // Keep consonant with serialize()
    prodIndex = ProdIndex::deserialize(decoder, version);
    decoder.decode(prodSize);
    decoder.decode(chunkOffset);
}

bool ChunkInfo::operator==(const ChunkInfo& that) const noexcept
{
    return prodIndex == that.prodIndex && prodSize == that.prodSize &&
            chunkOffset == that.chunkOffset;
}

size_t ChunkInfo::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    // Keep consonant with ChunkInfo(Decoder, unsigned)
    return encoder.encode(prodIndex) + encoder.encode(prodSize) +
            encoder.encode(chunkOffset);
}

ChunkInfo ChunkInfo::deserialize(
        Decoder&          decoder,
        const unsigned    version)
{
    return ChunkInfo(decoder, version);
}

} // namespace
