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
        const char*    buf,
        const size_t   size,
        const unsigned version)
    : prodIndex(buf, size, version),
      chunkIndex(0)
{
    // Keep consonant with serialize()
    unsigned expect = ChunkInfo::getSerialSize(version);
    if (size < expect)
        throw std::invalid_argument("Serialized chunk-information has too few "
                "bytes: expected " + std::to_string(expect) + ", actual="
                + std::to_string(size));
    buf += prodIndex.getSerialSize(version);
    chunkIndex = ntohl(*reinterpret_cast<const uint32_t*>(buf));
}

bool ChunkInfo::operator==(const ChunkInfo& that) const noexcept
{
    return prodIndex == that.prodIndex && chunkIndex == that.chunkIndex;
}

char* ChunkInfo::serialize(
            char*          buf,
            const size_t   size,
            const unsigned version) const
{
    // Keep consonant with ChunkInfo()
    unsigned expect = ChunkInfo::getSerialSize(version);
    if (size < expect)
        throw std::invalid_argument("Serialized chunk-information buffer is "
                "too small: need " + std::to_string(expect) + " bytes, actual="
                + std::to_string(size));
    buf = prodIndex.serialize(buf, size, version);
    *reinterpret_cast<ChunkIndex*>(buf) = htonl(chunkIndex);
    return buf + sizeof(ChunkIndex);
}

ChunkInfo ChunkInfo::deserialize(
        const char* const buf,
        const size_t      size,
        const unsigned    version)
{
    return ChunkInfo(buf, size, version);
}

} // namespace
