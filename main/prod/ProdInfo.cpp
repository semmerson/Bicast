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
#include "ProdIndex.h"
#include "ProdInfo.h"
#include "ProdSize.h"
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
    ProdIndex   index;
    ProdSize    size;
    ChunkSize   canonChunkSize;
    ProdName    name;

public:
    /**
     * Default constructs.
     */
    Impl()
        : index{0}
        , size{0}
        , canonChunkSize{0}
        , name{}
    {}

    Impl(   const ProdIndex index,
            const ProdName& name,
            const ProdSize  size,
            const ChunkSize canonChunkSize)
        : index{index}
        , size{size}
        , canonChunkSize{canonChunkSize}
        , name{name}
    {}

    Impl(   Decoder&        decoder,
            const unsigned  version)
        : Impl{}
    {
        // Keep consonant with ProdInfo::serialize()
        size = ProdSize::deserialize(decoder, version);
        index = ProdIndex::deserialize(decoder, version);
        canonChunkSize = ChunkSize::deserialize(decoder, version);
        name = ProdName::deserialize(decoder, version);
    }

    Impl(const Impl& impl) =delete;
    Impl(const Impl&& impl) =delete;
    Impl& operator=(const Impl& rhs) =delete;
    Impl& operator=(const Impl&& rhs) =delete;

    /**
     * Returns a string representation of this instance.
     * @return String representation of this instance
     */
    std::string to_string() const
    {
        return "{index=" + index.to_string() + ", name=\"" + name.to_string() +
                "\", size=" + std::to_string(size) + ", canonicalChunkSize=" +
                canonChunkSize.to_string() + "}";
    }

    /**
     * Returns the name of the product.
     * @return Name of the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    inline const ProdName getName() const
    {
        return name;
    }

    /**
     * Returns the index of the product.
     * @return index of the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    inline ProdIndex getIndex() const
    {
        return index;
    }

    /**
     * Returns the size of the product.
     * @return          Size of the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    inline ProdSize getSize() const
    {
        return size;
    }

    /**
     * Indicates if this instance is earlier than another.
     * @param[in] that   Other instance
     * @retval `true`    Yes
     * @retval `false`   No
     */
    inline bool isEarlierThan(const Impl& that) const noexcept
    {
        return index.isEarlierThan(that.index);
    }

    /**
     * Returns the size of the product's canonical data chunks.
     * @return          Size of canonical data chunks
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    inline ChunkSize getChunkSize() const noexcept
    {
        return canonChunkSize;
    }

    ChunkSize getChunkSize(const ChunkIndex index) const
    {
        auto offset = getOffset(index);
        return (offset + canonChunkSize <= size)
                ? canonChunkSize
                : ChunkSize{static_cast<ChunkSize::type>(size - offset)};
    }

    /**
     * Returns the number of chunks in the product.
     * @return the number of chunks in the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    inline ChunkIndex getNumChunks() const noexcept
    {
        return (size + canonChunkSize - 1) / canonChunkSize;
    }

    ChunkIndex getChunkIndex(const ChunkOffset offset) const
    {
        if (offset % canonChunkSize)
            throw INVALID_ARGUMENT("Invalid chunk-offset: offset=" +
                    offset.to_string() + ", canonChunkSize=" +
                    canonChunkSize.to_string());
        return offset / canonChunkSize;
    }

    void vet(const ChunkIndex chunkIndex) const
    {
        auto numChunks = getNumChunks();
        if (numChunks <= chunkIndex)
            throw INVALID_ARGUMENT("Invalid chunk index: index=" +
                    std::to_string(chunkIndex) + ", numChunks=" +
                    std::to_string(numChunks));
    }

    ChunkOffset getOffset(const ChunkIndex index) const
    {
        vet(index);
        return index * canonChunkSize;
    }

    bool equalsExceptName(const Impl& that) const noexcept
    {
        const bool result = ((index == that.index) &&
                (size == that.size) &&
                (canonChunkSize == that.canonChunkSize));
        return result;
    }

    /**
     * Indicates if this instance is considered equal to another.
     * @param[in] that  Other instance
     * @retval true     Instance is equal to other
     * @retval false    Instance is not equal to other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator==(const Impl& that) const noexcept
    {
        return equalsExceptName(that) &&
                (name.compare(that.name) == 0);
    }

    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return             Number of bytes in serial representation
     */
    size_t getSerialSize(unsigned version) const noexcept
    {
        // Keep consonant with serialize()
        return  size.getSerialSize(version) +
                index.getSerialSize(version) +
                canonChunkSize.getSerialSize(version) +
                name.getSerialSize(version);
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
        // Keep consonant with ProdInfo::deserialize()
        return  size.serialize(encoder, version) +
                index.serialize(encoder, version) +
                canonChunkSize.serialize(encoder, version) +
                name.serialize(encoder, version);
    }
};

ProdInfo::ProdInfo()
    : pImpl{}
{}

ProdInfo::ProdInfo(
        const ProdIndex index,
        const ProdName& name,
        const ProdSize  size,
        const ChunkSize canonChunkSize)
    : pImpl(new Impl(index, name, size, canonChunkSize))
{}

ProdInfo::ProdInfo(
        const ProdIndex index,
        const ProdSize  size,
        const ChunkSize canonChunkSize)
    : ProdInfo{index, "", size, canonChunkSize}
{}

ProdInfo::operator bool() const noexcept
{
    return pImpl.operator bool();
}

bool ProdInfo::isComplete() const noexcept
{
    return pImpl.operator bool() && pImpl->getName().length() != 0;
}

std::string ProdInfo::to_string() const
{
    return pImpl->to_string();
}

bool ProdInfo::equalsExceptName(const ProdInfo& that) const noexcept
{
    const bool result = pImpl->equalsExceptName(*that.pImpl.get());
    return result;
}

bool ProdInfo::operator==(const ProdInfo& that) const noexcept
{
    return pImpl->operator==(*that.pImpl.get());
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

const ProdName ProdInfo::getName() const
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

ChunkIndex ProdInfo::getNumChunks() const noexcept
{
    return pImpl->getNumChunks();
}

ChunkIndex ProdInfo::getChunkIndex(const ChunkOffset offset) const
{
    return pImpl->getChunkIndex(offset);
}

ChunkIndex ProdInfo::getChunkIndex(const ChunkId chunkId) const
{
    return chunkId.getChunkIndex();
}

ChunkOffset ProdInfo::getChunkOffset(const ChunkIndex index) const
{
    return pImpl->getOffset(index);
}

ChunkOffset ProdInfo::getChunkOffset(const ChunkId chunkId) const
{
    return pImpl->getOffset(chunkId.getChunkIndex());
}

void ProdInfo::vet(const ChunkIndex chunkIndex) const
{
    return pImpl->vet(chunkIndex);
}

ChunkInfo ProdInfo::getChunkInfo(const ChunkIndex chunkIndex) const
{
    return ChunkInfo(*this, chunkIndex);
}

ChunkId ProdInfo::makeChunkId(const ChunkOffset offset) const
{
    return ChunkId{*this, getChunkIndex(offset)};
}

ChunkId ProdInfo::makeChunkId(const ChunkIndex index) const
{
    return ChunkId{*this, index};
}

ProdInfo ProdInfo::deserialize(
        Decoder&        decoder,
        const unsigned  version)
{
    Impl impl{decoder, version};
    return ProdInfo{impl.getIndex(), impl.getName(), impl.getSize(),
            impl.getChunkSize()};
}

} // namespace
