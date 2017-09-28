/**
 * This file defines information on a product.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfo.cpp
 * @author: Steven R. Emmerson
 */

#include "Chunk.h"
#include "error.h"
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

class ProdInfo::Impl final
{
    std::string name;
    ProdIndex   index;
    ProdSize    size;

public:
    /**
     * Default constructs.
     */
    Impl()
        : name()
        , index(0)
        , size(0)
    {}

    /**
     * Constructs from information on a product.
     * @param[in] name       Product name
     * @param[in] index      Product index
     * @param[in] size       Size of product in bytes
     * @throws std::invalid_argument if `name.size() > prodNameSizeMax`
     */
    Impl(
            const std::string& name,
            const ProdIndex    index,
            const ProdSize     size)
        : name(name)
        , index(index)
        , size(size)
    {
        if (name.size() > UINT16_MAX)
            throw INVALID_ARGUMENT("Name too long: " +
                    std::to_string(name.size()) + " bytes");
    }

    /**
     * Returns a string representation of this instance.
     * @return String representation of this instance
     */
    std::string to_string() const
    {
        return "{name=\"" + name + "\", index=" + std::to_string(index)
                + ", size=" + std::to_string(size) + "}";
    }

    /**
     * Constructs by deserializing a serialized representation from a decoder.
     * @param[in] decoder  Decoder
     * @param[in] version  Serialization version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    Impl(
        Decoder&        decoder,
        const unsigned  version)
        : name()
        , index(0)
        , size(0)
    {
        // Keep consonant with ProdInfo::serialize()
        index = ProdIndex::deserialize(decoder, version);
        decoder.decode(size);
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
     * Indicates if this instance is earlier than another.
     * @param[in] that   Other instance
     * @retval `true`    Yes
     * @retval `false`   No
     */
    bool isEarlierThan(const Impl& that) const noexcept
    {
        return index < that.index;
    }

    /**
     * Returns the size of the product's data chunks in bytes.
     * @return Size of the product's data chunks in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkSize getChunkSize() const
    {
        return ChunkInfo::getCanonSize();
    }

    /**
     * Returns the size, in bytes, of a given chunk-of-data.
     * @param[in] index  Index of the chunk
     * @return           The size of the chunk in bytes
     * @throws InvalidArgument  The chunk index is invalid
     * @execeptionsafety Strong guarantee
     * @threadsafety     Safe
     */
    ChunkSize getChunkSize(ChunkIndex index) const
    {
        if (index >= getNumChunks())
            throw INVALID_ARGUMENT("Invalid chunk-index: max=" +
                    std::to_string(getNumChunks()-1) + ", index=" +
                    std::to_string(index));
        return (index + 1 < getNumChunks())
                ? ChunkInfo::getCanonSize()
                : size - index*ChunkInfo::getCanonSize();
    }

    /**
     * Returns the number of chunks in the product.
     * @return the number of chunks in the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkIndex getNumChunks() const
    {
        return (size+ChunkInfo::getCanonSize()-1)/ChunkInfo::getCanonSize();
    }

    /**
     * Returns the offset, in bytes, from the start of the data-product's data
     * to the data of the chunk.
     * @param[in] chunkIndex  Origin-0 index of a chunk of data
     * @return Offset to chunk's data from start of product's data
     * @throws InvalidArgument Chunk-index is greater than or equal to the
     *                         number of chunks
     */
    ChunkOffset getOffset(const ChunkIndex chunkIndex) const
    {
        if (chunkIndex >= getNumChunks())
            throw INVALID_ARGUMENT("Chunk-index is too great: index=" +
                    std::to_string(chunkIndex) + ", numChunks=" +
                    std::to_string(getNumChunks()));
        return ChunkInfo::getOffset(chunkIndex);
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
            throw INVALID_ARGUMENT("Wrong product-index: expected=" +
                    std::to_string(index) + ", actual=" +
                    std::to_string(chunkInfo.getProdIndex()));
        if (chunkSize != chunkInfo.getSize())
            throw INVALID_ARGUMENT("Unexpected chunk size: expected=" +
                    std::to_string(chunkInfo.getSize()) +
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
    bool operator==(const Impl& that) const noexcept
    {
        return (index == that.index) &&
                (size == that.size) &&
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
        return  index.getSerialSize(version) +
                Codec::getSerialSize(&size) +
                Codec::getSerialSize(name);
    }

    /**
     * Serializes this instance to an encoder.
     * @param[in] encoder   Encoder
     * @param[in] version   Serialization version
     * @execptionsafety Basic guarantee
     * @threadsafety    Compatible but not thread-safe
     */
    size_t serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        // Keep consonant with ProdInfo::ProdInfo()
        return encoder.encode(index) +
                encoder.encode(size) +
                encoder.encode(name);
    }
};

ProdInfo::ProdInfo()
    : pImpl(new Impl())
{}

ProdInfo::ProdInfo(
        const std::string& name,
        const ProdIndex    index,
        const ProdSize     size)
    : pImpl(new Impl(name, index, size))
{}

std::string ProdInfo::to_string() const
{
    return pImpl->to_string();
}

bool ProdInfo::operator==(const ProdInfo& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

size_t ProdInfo::getSerialSize(unsigned version) const noexcept
{
    return pImpl->getSerialSize(version);
}

size_t ProdInfo::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    return pImpl->serialize(encoder, version);
}

const std::string& ProdInfo::getName() const
{
    return pImpl->getName();
}

ProdIndex ProdInfo::getIndex() const noexcept
{
    return pImpl->getIndex();
}

ProdSize ProdInfo::getSize() const noexcept
{
    return pImpl->getSize();
}

bool ProdInfo::isEarlierThan(const ProdInfo& that) const noexcept
{
    return pImpl->isEarlierThan(*that.pImpl.get());
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

ChunkOffset ProdInfo::getOffset(const ChunkIndex chunkIndex) const
{
    return pImpl->getOffset(chunkIndex);
}

void ProdInfo::vet(
        const ChunkInfo& chunkInfo,
        const ChunkSize  chunkSize) const
{
    return pImpl->vet(chunkInfo, chunkSize);
}

ChunkInfo ProdInfo::makeChunkInfo(const ChunkIndex chunkIndex) const
{
    return ChunkInfo(*this, chunkIndex);
}

ProdInfo ProdInfo::deserialize(
        Decoder&        decoder,
        const unsigned  version)
{
    auto impl = Impl(decoder, version);
    return ProdInfo(impl.getName(), impl.getIndex(), impl.getSize());
}

} // namespace
