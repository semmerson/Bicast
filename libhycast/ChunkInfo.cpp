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
        const void*    buf,
        const size_t   size,
        const unsigned version)
{
    // Keep consonant with serialize()
    unsigned expect = ChunkInfo::getSerialSize(version);
    if (size < expect)
        throw std::invalid_argument("Serialized chunk-information has too few "
                "bytes: expected " + std::to_string(expect) + ", actual="
                + std::to_string(size));
    const uint32_t* uint32s = reinterpret_cast<const uint32_t*>(buf);
    prodIndex = ntohl(uint32s[0]);
    chunkIndex = ntohl(uint32s[1]);
}

std::shared_ptr<ChunkInfo> ChunkInfo::create(
        const void*    buf,
        const size_t   size,
        const unsigned version)
{
    return std::shared_ptr<ChunkInfo>(new ChunkInfo(buf, size, version));
}

bool ChunkInfo::equals(const ChunkInfo& that) const
{
    return prodIndex == that.prodIndex && chunkIndex == that.chunkIndex;
}

void ChunkInfo::serialize(
            void* const    buf,
            const size_t   size,
            const unsigned version) const
{
    // Keep consonant with ChunkInfo()
    unsigned expect = ChunkInfo::getSerialSize(version);
    if (size < expect)
        throw std::invalid_argument("Serialized chunk-information buffer is "
                "too small: need " + std::to_string(expect) + " bytes, actual="
                + std::to_string(size));
    uint32_t* uint32s = reinterpret_cast<uint32_t*>(buf);
    uint32s[0] = htonl(prodIndex);
    uint32s[1] = htonl(chunkIndex);
}

std::shared_ptr<ChunkInfo> ChunkInfo::deserialize(
        const void* const buf,
        const size_t      size,
        const unsigned    version)
{
    return std::shared_ptr<ChunkInfo>(new ChunkInfo(buf, size, version));
}

} // namespace
