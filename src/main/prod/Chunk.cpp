/**
 * This file implements two types of chunks of data: 1) a latent chunk that must
 * be read from an object channel; and 2) a reified chunk with a pointer to its
 * data. The two types are in the same file to support keeping their
 * serialization and de-serialization methods consistent.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Chunk.cpp
 * @author: Steven R. Emmerson
 */

#include "Chunk.h"
#include "ChunkInfo.h"
#include "HycastTypes.h"
#include "RecStream.h"
#include "SctpSock.h"

namespace hycast {

class LatentChunkImpl final
{
    ChunkInfo    info;
    Decoder*     decoder;
    ChunkSize    size;
    unsigned     version;

public:
    /**
     * Constructs from nothing.
     */
    LatentChunkImpl()
        : info(),
          decoder(nullptr),
          size(0),
          version(0)
    {}

    /**
     * Constructs from a serialized representation in a decoder.
     * @param[in] decoder   Decoder. *Must* exist for the duration of this
     *                      instance
     * @param[in] version   Protocol version
     */
    LatentChunkImpl(
            Decoder&       decoder,
            const unsigned version)
        // Keep consistent with ActualChunkImpl::serialize()
        : info(ChunkInfo::deserialize(decoder, version)),
          decoder(&decoder),
          size(decoder.getDmaSize()),
          version(version)
    {}

    /**
     * Returns information on the chunk.
     * @return information on the chunk
     * @exceptionsafety Strong
     * @threadsafety Safe
     */
    const ChunkInfo& getInfo() const noexcept
    {
        return info;
    }

    /**
     * Returns the index of the associated product.
     * @return the index of the associated product
     */
    ProdIndex_t getProdIndex() const noexcept
    {
        return info.getProdIndex();
    }

    /**
     * Returns the index of the chunk-of-data.
     * @return the index of the chunk
     */
    ProdIndex_t getChunkIndex() const noexcept
    {
        return info.getChunkIndex();
    }

    /**
     * Returns the size of the chunk of data.
     * @return the size of the chunk of data
     * @exceptionsafety Strong
     * @threadsafety Safe
     */
    ChunkSize getSize() const
    {
        return size;
    }

    /**
     * Drains the chunk of data into a buffer. The latent data will no longer
     * be available.
     * @param[in] data  Buffer to drain the chunk of data into
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    void drainData(void* data)
    {
        decoder->decode(data, size);
    }

    /**
     * Discards the chunk of data. The latent data will no longer be available.
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    void discard()
    {
        decoder->discard();
    }

    /**
     * Indicates if this instance has data (i.e., whether or not `drainData()`
     * has been called).
     * @retval true   This instance has data
     * @retval false  This instance doesn't have data
     */
    bool hasData()
    {
        return decoder->hasRecord();
    }
};

class ActualChunkImpl final
{
    ChunkInfo   info;
    const void* data;
    ChunkSize   size;
public:
    /**
     * Constructs from nothing.
     */
    ActualChunkImpl()
        : info(),
          data(nullptr),
          size(0)
    {}

    /**
     * Constructs from information on the chunk and a pointer to its data.
     * @param[in] info  Chunk information
     * @param[in] data  Chunk data
     * @param[in] size  Amount of data in bytes
     */
    ActualChunkImpl(
            const ChunkInfo& info,
            const void*      data,
            const ChunkSize  size)
        : info(info),
          data(data),
          size(size)
    {}

    /**
     * Returns information on the chunk.
     * @return information on the chunk
     * @exceptionsafety Nothrow
     * @threadsafety Safe
     */
    const ChunkInfo& getInfo() const noexcept
    {
        return info;
    }

    /**
     * Returns the index of the associated product.
     * @return the index of the associated product
     */
    ProdIndex_t getProdIndex() const noexcept
    {
        return info.getProdIndex();
    }

    /**
     * Returns the index of the chunk-of-data.
     * @return the index of the chunk
     */
    ChunkIndex getChunkIndex() const noexcept
    {
        return info.getChunkIndex();
    }

    /**
     * Returns the size of the chunk of data.
     * @return the size of the chunk of data
     * @exceptionsafety Nothrow
     * @threadsafety Safe
     */
    ChunkSize getSize() const noexcept
    {
        return size;
    }

    /**
     * Returns a pointer to the data.
     * @returns a pointer to the data
     * @exceptionsafety Nothrow
     * @threadsafety Safe
     */
    const void* getData() const noexcept
    {
        return data;
    }

    /**
     * Serializes this instance to an encoder.
     * @param[in] encoder   Encoder
     * @param[in] version   Protocol version
     * @exceptionsafety Basic
     * @threadsafety Compatible but not safe
     */
    void serialize(
            Encoder&       encoder,
            const unsigned version)
    {
        /*
         * Keep consistent with `LatentChunkImpl::LatentChunkImpl(Decoder,
         * unsigned)`
         */
        info.serialize(encoder, version);
        encoder.encode(data, size);
    }
};

ActualChunk::ActualChunk()
    : pImpl(new ActualChunkImpl())
{
}

ActualChunk::ActualChunk(
        const ChunkInfo& info,
        const void*      data,
        const ChunkSize  size)
    : pImpl(new ActualChunkImpl(info, data, size))
{}

const ChunkInfo& ActualChunk::getInfo() const noexcept
{
    return pImpl->getInfo();
}

ProdIndex_t ActualChunk::getProdIndex() const noexcept
{
    return pImpl->getProdIndex();
}

ProdIndex_t ActualChunk::getChunkIndex() const noexcept
{
    return pImpl->getChunkIndex();
}

ChunkSize ActualChunk::getSize() const noexcept
{
    return pImpl->getSize();
}

const void* ActualChunk::getData() const noexcept
{
    return pImpl->getData();
}

void ActualChunk::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    pImpl->serialize(encoder, version);
}

LatentChunk::LatentChunk()
    : pImpl(new LatentChunkImpl())
{}

LatentChunk::LatentChunk(
        Decoder&       decoder,
        const unsigned version)
    : pImpl(new LatentChunkImpl(decoder, version))
{}

LatentChunk LatentChunk::deserialize(
        Decoder&       decoder,
        const unsigned version)
{
    return LatentChunk(decoder, version);
}

const ChunkInfo& LatentChunk::getInfo() const noexcept
{
    return pImpl->getInfo();
}

ProdIndex_t LatentChunk::getProdIndex() const noexcept
{
    return pImpl->getProdIndex();
}

ProdIndex_t LatentChunk::getChunkIndex() const noexcept
{
    return pImpl->getChunkIndex();
}

ChunkSize LatentChunk::getSize() const noexcept
{
    return pImpl->getSize();
}

void LatentChunk::drainData(void* data)
{
    pImpl->drainData(data);
}

void LatentChunk::discard()
{
    pImpl->discard();
}

bool LatentChunk::hasData()
{
    return pImpl->hasData();
}

} // namespace
