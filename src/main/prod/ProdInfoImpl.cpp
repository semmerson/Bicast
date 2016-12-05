/**
 * This file defines an implementation of information on a product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfo.cpp
 * @author: Steven R. Emmerson
 */

#include "HycastTypes.h"
#include "ProdIndex.h"
#include "ProdInfoImpl.h"

#include <arpa/inet.h>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <string>

namespace hycast {

ProdInfoImpl::ProdInfoImpl(
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

ProdInfoImpl::ProdInfoImpl(
        const char* const buf,
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

ChunkSize ProdInfoImpl::getChunkSize(ChunkIndex index) const
{
    if (index >= getNumChunks())
        throw std::invalid_argument("Invalid chunk-index: max=" +
                std::to_string(getNumChunks()-1) + ", index=" +
                std::to_string(index));
    return (index + 1 < getNumChunks())
            ? chunkSize
            : size - index*chunkSize;
}

void ProdInfoImpl::vet(
        const ChunkInfo& chunkInfo,
        const ChunkSize  chunkSize) const
{
    if (chunkInfo.getProdIndex() != index)
        throw std::invalid_argument("Wrong product-index: expected=" +
                std::to_string(index) + ", actual=" +
                std::to_string(chunkInfo.getProdIndex()));
    if (chunkSize != getChunkSize(chunkInfo.getChunkIndex()))
        throw std::invalid_argument("Unexpected chunk size: expected=" +
                std::to_string(getChunkSize(chunkInfo.getChunkIndex())) +
                ", actual=" + std::to_string(chunkSize));
}

bool ProdInfoImpl::operator==(const ProdInfoImpl& that) const noexcept
{
    return (index == that.index) &&
            (size == that.size) &&
            (chunkSize == that.chunkSize) &&
            (name.compare(that.name) == 0);
}

size_t ProdInfoImpl::getSerialSize(unsigned version) const noexcept
{
    // Keep consonant with serialize()
    return 2*sizeof(uint32_t) + 2*sizeof(uint16_t) + name.size();
}

char* ProdInfoImpl::serialize(
        char*          buf,
        const size_t   bufLen,
        const unsigned version) const
{
    size_t nbytes = getSerialSize(version);
    if (bufLen < nbytes)
        throw std::invalid_argument("Buffer too small for serialized product "
                "information: need=" + std::to_string(nbytes) + " bytes, bufLen="
                + std::to_string(bufLen));
    // Keep consonant with ProdInfo::ProdInfo()
    buf = index.serialize(buf, bufLen, version);
    *reinterpret_cast<uint32_t*>(buf) = htonl(size);
    buf += 4;
    *reinterpret_cast<uint16_t*>(buf) = htons(chunkSize);
    buf += 2;
    *reinterpret_cast<uint16_t*>(buf) =
            htons(static_cast<uint16_t>(name.size()));
    buf += 2;
    (void)memcpy(buf, name.data(), name.size());
    return buf + name.size();
}

std::shared_ptr<ProdInfoImpl> ProdInfoImpl::deserialize(
        const char* const buf,
        const size_t      size,
        const unsigned    version)
{
    return std::shared_ptr<ProdInfoImpl>(new ProdInfoImpl(buf, size, version));
}

} // namespace
