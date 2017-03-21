/**
 * This file implements a multicast datagram for a chunk-of-data -- one that
 * contains all necessary information to fully-allocate its associated data-
 * product in the absence of complete information on the product.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkGram.cpp
 * @author: Steven R. Emmerson
 */

#include "config.h"

#include "ChunkGram.h"

namespace hycast {

class ChunkGram::Impl final : public Serializable<ChunkGram::Impl>
{
    ChunkInfo chunkInfo;
    ProdSize  prodSize;

public:
    Impl(   const ChunkInfo& chunkInfo,
            const ProdSize   prodSize)
        : chunkInfo{chunkInfo}
        , prodSize{prodSize}
    {}

    /**
     * Returns information on the associated chunk-of-data.
     * @return Information on the associated chunk-of-data
     */
    ChunkInfo getChunkInfo() const noexcept
    {
        return chunkInfo;
    }

    /**
     * Returns the size of the associated data-product in bytes.
     * @return Size of the associated data-product in bytes
     */
    ProdSize getProdSize() const noexcept
    {
        return prodSize;
    }

    /**
     * Returns the size, in bytes, of a serialized representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the size, in bytes, of a serialized representation of this
     *         instance
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t getSerialSize(unsigned version) const noexcept
    {
        // Keep consonant with `serialize()`
        return chunkInfo.getSerialSize(version) +
                Codec::getSerialSize(&prodSize);
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
        // Keep consonant with `getSerialSize()`
        return chunkInfo.serialize(encoder, version) +
                encoder.encode(prodSize);
    }

    /**
     * Returns an instance corresponding to the serialized representation in a
     * decoder.
     * @param[in] decoder    Decoder
     * @param[in] version    Protocol version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    static Impl deserialize(
            Decoder&        decoder,
            const unsigned  version)
    {
        const auto chunkInfo = ChunkInfo::deserialize(decoder, version);
        ProdSize   prodSize;
        decoder.decode(prodSize);
        return Impl(chunkInfo, prodSize);
    }
};

ChunkGram::ChunkGram(
        const ChunkInfo& chunkInfo,
        const ProdSize   prodSize)
    : pImpl{new Impl(chunkInfo, prodSize)}
{}

size_t ChunkGram::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    return pImpl->serialize(encoder, version);
}

ChunkInfo ChunkGram::getChunkInfo() const noexcept
{
    return pImpl->getChunkInfo();
}

ProdSize ChunkGram::getProdSize() const noexcept
{
    return pImpl->getProdSize();
}

size_t ChunkGram::getSerialSize(unsigned version) const noexcept
{
    return pImpl->getSerialSize(version);
}

ChunkGram ChunkGram::deserialize(
        Decoder&        decoder,
        const unsigned  version)
{
    const auto impl = Impl::deserialize(decoder, version);
    return ChunkGram(impl.getChunkInfo(), impl.getProdSize());
}

} // namespace
