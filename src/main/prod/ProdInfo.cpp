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

#include "Chunk.h"
#include "HycastTypes.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include "Serializable.h"

#include <arpa/inet.h>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdint>
#include <memory>
#include <string>

namespace hycast {

class ProdInfoImpl : public Serializable<ProdInfoImpl>
{
    std::string name;
    ProdIndex   index;
    ProdSize    size;
    ChunkSize   chunkSize;
public:
    /**
     * Constructs from information on a product.
     * @param[in] name       Product name
     * @param[in] index      Product index
     * @param[in] size       Size of product in bytes
     * @param[in] chunkSize  Size of data chunks in bytes
     * @throws std::invalid_argument if `name.size() > prodNameSizeMax`
     */
    ProdInfoImpl(
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

    /**
     * Constructs by deserializing a serialized representation from a buffer.
     * @param[in] buf      Buffer
     * @param[in] version  Serialization version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    ProdInfoImpl(
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

    /**
     * Returns the name of the product.
     * @return Name of the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const std::string& getName() const
    {
        return name;
    }

    /**
     * Returns the index of the product.
     * @return index of the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ProdIndex getIndex() const
    {
        return index;
    }

    /**
     * Returns the size of the product in bytes.
     * @return Size of the product in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ProdSize getSize() const
    {
        return size;
    }

    /**
     * Returns the size of the product's data chunks in bytes.
     * @return Size of the product's data chunks in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkSize getChunkSize() const
    {
        return chunkSize;
    }

    /**
     * Returns the size, in bytes, of a given chunk-of-data.
     * @param[in] index  Index of the chunk
     * @return           The size of the chunk in bytes
     * @throws std::invalid_argument if the index is invalid
     * @execeptionsafety Strong guarantee
     * @threadsafety     Safe
     */
    ChunkSize getChunkSize(ChunkIndex index) const
    {
        if (index >= getNumChunks())
            throw std::invalid_argument("Invalid chunk-index: max=" +
                    std::to_string(getNumChunks()-1) + ", index=" +
                    std::to_string(index));
        return (index + 1 < getNumChunks())
                ? chunkSize
                : size - index*chunkSize;
    }

    /**
     * Returns the number of chunks in the product.
     * @return the number of chunks in the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkIndex getNumChunks() const
    {
        return (size+chunkSize-1)/chunkSize;
    }

    /**
     * Vets information on a chunk-of-data ostensibly belonging to this
     * instance's associated product.
     * @param[in] chunkInfo  Information on the chunk
     * @param[in] chunkSize  Size of the chunk in bytes
     * @throws std::invalid_argument if the chunk is inconsistent with this
     *                               instance's product
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void vet(
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

    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance
     * @retval true   This instance is equal to the other
     * @retval false  This instance is not equal to the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator==(const ProdInfoImpl& that) const noexcept
    {
        return (index == that.index) &&
                (size == that.size) &&
                (chunkSize == that.chunkSize) &&
                (name.compare(that.name) == 0);
    }

    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    size_t getSerialSize(unsigned version) const noexcept
    {
        // Keep consonant with serialize()
        return 2*sizeof(uint32_t) + 2*sizeof(uint16_t) + name.size();
    }

    /**
     * Serializes this instance to a buffer.
     * @param[in] buf       Buffer
     * @param[in] bufLen    Buffer size in bytes
     * @param[in] version   Serialization version
     * @return Address of next byte
     * @execptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    char* serialize(
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
};

ProdInfo::ProdInfo(
        const std::string& name,
        const ProdIndex    index,
        const ProdSize     size,
        const ChunkSize    chunkSize)
    : pImpl(new ProdInfoImpl(name, index, size, chunkSize))
{}

ProdInfo::ProdInfo(
        const char* const buf,
        const size_t      bufLen,
        const unsigned    version)
    : pImpl(new ProdInfoImpl(buf, bufLen, version))
{}

bool ProdInfo::operator==(const ProdInfo& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

size_t ProdInfo::getSerialSize(unsigned version) const noexcept
{
    return pImpl->getSerialSize(version);
}

char* ProdInfo::serialize(
        char*          buf,
        const size_t   bufLen,
        const unsigned version) const
{
    return pImpl->serialize(buf, bufLen, version);
}

const std::string& ProdInfo::getName() const
{
    return pImpl->getName();
}

ProdIndex ProdInfo::getIndex() const
{
    return pImpl->getIndex();
}

ProdSize ProdInfo::getSize() const
{
    return pImpl->getSize();
}

ChunkSize ProdInfo::getChunkSize() const
{
    return pImpl->getChunkSize();
}

ChunkSize ProdInfo::getChunkSize(const ChunkIndex index) const
{
    return pImpl->getChunkSize(index);
}

ChunkIndex ProdInfo::getNumChunks() const
{
    return pImpl->getNumChunks();
}

void ProdInfo::vet(
        const ChunkInfo& chunkInfo,
        const ChunkSize  chunkSize) const
{
    return pImpl->vet(chunkInfo, chunkSize);
}

ProdInfo ProdInfo::deserialize(
        const char* const buf,
        const size_t      size,
        const unsigned    version)
{
    return ProdInfo(buf, size, version);
}

} // namespace
