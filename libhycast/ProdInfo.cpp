/**
 * This file defines information on a product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfo.cpp
 * @author: Steven R. Emmerson
 */

#include "HycastTypes.h"
#include "ProdInfo.h"

#include <arpa/inet.h>
#include <stdexcept>
#include <cstdint>
#include <cstring>

namespace hycast {

ProdInfo::ProdInfo()
    : name(""),
      index(0),
      size(0),
      chunkSize(0)
{
}

ProdInfo::ProdInfo(
        const std::string& name,
        const ProdIndex    index,
        const ProdSize     size,
        const ChunkSize    chunkSize)
    : name(name),
      index(index),
      size(size),
      chunkSize(chunkSize)
{
    if (name.size() > UINT16_MAX)
        throw std::invalid_argument("Name too long: " +
                std::to_string(name.size()) + " bytes");
}

ProdInfo::ProdInfo(
        const void* const buf,
        const size_t      bufLen,
        const unsigned    version)
    : name(),
      index(0),
      size(0),
      chunkSize(0)
{
    size_t nbytes = getSerialSize(version);
    if (bufLen < nbytes)
        throw std::invalid_argument("Buffer too small for serialized product "
                "information: need=" + std::to_string(nbytes) + " bytes, bufLen="
                + std::to_string(bufLen));
    // Keep consonant with ProdInfo::serialize()
    const uint8_t* const bytes = reinterpret_cast<const uint8_t*>(buf);
    index = ntohl(*reinterpret_cast<const uint32_t*>(bytes));
    size = ntohl(*reinterpret_cast<const uint32_t*>(bytes+4));
    chunkSize = ntohs(*reinterpret_cast<const uint16_t*>(bytes+8));
    const size_t nameLen = ntohs(*reinterpret_cast<const uint16_t*>(bytes+10));
    if (nameLen > bufLen - nbytes)
        throw std::invalid_argument("Buffer too small for product name: need=" +
                std::to_string(nameLen) + " bytes, remaining=" +
                std::to_string(bufLen-nbytes));
    char nameBuf[nameLen];
    (void)memcpy(nameBuf, bytes+12, nameLen);
    name.assign(nameBuf, nameLen);
}

std::shared_ptr<ProdInfo> ProdInfo::create(
        const void* const buf,
        const size_t      size,
        const unsigned    version)
{
    return std::shared_ptr<ProdInfo>(new ProdInfo(buf, size, version));
}

bool ProdInfo::equals(const ProdInfo& that) const
{
    return (index == that.index) &&
            (size == that.size) &&
            (chunkSize == that.chunkSize) &&
            (name.compare(that.name) == 0);
}

size_t ProdInfo::getSerialSize(unsigned version) const
{
    // Keep consonant with serialize()
    return 2*sizeof(uint32_t) + 2*sizeof(uint16_t) + name.size();
}

void ProdInfo::serialize(
        void* const    buf,
        const size_t   bufLen,
        const unsigned version) const
{
    size_t nbytes = getSerialSize(version);
    if (bufLen < nbytes)
        throw std::invalid_argument("Buffer too small for serialized product "
                "information: need=" + std::to_string(nbytes) + " bytes, bufLen="
                + std::to_string(bufLen));
    // Keep consonant with ProdInfo::ProdInfo()
    uint8_t* const bytes = reinterpret_cast<uint8_t*>(buf);
    *reinterpret_cast<uint32_t*>(bytes) = htonl(index);
    *reinterpret_cast<uint32_t*>(bytes+4) = htonl(size);
    *reinterpret_cast<uint16_t*>(bytes+8) = htons(chunkSize);
    *reinterpret_cast<uint16_t*>(bytes+10) =
            htons(static_cast<uint16_t>(name.size()));
    (void)memcpy(bytes+12, name.data(), name.size());
}

std::shared_ptr<ProdInfo> ProdInfo::deserialize(
        const void* const buf,
        const size_t      size,
        const unsigned    version)
{
    return std::shared_ptr<ProdInfo>(new ProdInfo(buf, size, version));
}

} // namespace
