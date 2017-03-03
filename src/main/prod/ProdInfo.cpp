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

class ProdInfoImpl final
{
    std::string name;
    ProdIndex_t index;
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
     * Constructs by deserializing a serialized representation from a decoder.
     * @param[in] decoder  Decoder
     * @param[in] version  Serialization version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    ProdInfoImpl(
        Decoder&        decoder,
        const unsigned  version)
        : name(),
          index(0),
          size(0),
          chunkSize(0)
    {
        // Keep consonant with ProdInfo::serialize()
        decoder.decode(index);
        decoder.decode(size);
        decoder.decode(chunkSize);
        decoder.decode(name);
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
        return  Codec::getSerialSize(&index) +
                Codec::getSerialSize(&size) +
                Codec::getSerialSize(&chunkSize) +
                Codec::getSerialSize(name);
    }

    /**
     * Serializes this instance to an encoder.
     * @param[in] encoder   Encoder
     * @param[in] version   Serialization version
     * @execptionsafety Basic guarantee
     * @threadsafety    Compatible but not thread-safe
     */
    void serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        // Keep consonant with ProdInfo::ProdInfo()
        encoder.encode(index);
        encoder.encode(size);
        encoder.encode(chunkSize);
        encoder.encode(name);
    }
};

ProdInfo::ProdInfo(
        const std::string& name,
        const ProdIndex    index,
        const ProdSize     size,
        const ChunkSize    chunkSize)
    : pImpl(new ProdInfoImpl(name, index, size, chunkSize))
{}

bool ProdInfo::operator==(const ProdInfo& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

size_t ProdInfo::getSerialSize(unsigned version) const noexcept
{
    return pImpl->getSerialSize(version);
}

void ProdInfo::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    pImpl->serialize(encoder, version);
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
        Decoder&        decoder,
        const unsigned  version)
{
    auto impl = ProdInfoImpl(decoder, version);
    return ProdInfo(impl.getName(), impl.getIndex(), impl.getSize(),
            impl.getChunkSize());
}

} // namespace
