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

#include <arpa/inet.h>

namespace hycast {

ChunkInfo::ChunkInfo(
        Decoder&       decoder,
        const unsigned version)
    : ChunkInfo()
{
    // Keep consonant with serialize()
    decoder.decode(prodIndex);
    decoder.decode(chunkIndex);
}

bool ChunkInfo::operator==(const ChunkInfo& that) const noexcept
{
    return prodIndex == that.prodIndex && chunkIndex == that.chunkIndex;
}

void ChunkInfo::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    // Keep consonant with ChunkInfo(Decoder, unsigned)
    encoder.encode(prodIndex);
    encoder.encode(chunkIndex);
}

ChunkInfo ChunkInfo::deserialize(
        Decoder&          decoder,
        const unsigned    version)
{
    return ChunkInfo(decoder, version);
}

} // namespace
