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
#include "error.h"
#include "HycastTypes.h"
#include "RecStream.h"
#include "SctpSock.h"

#include <cstring>

namespace hycast {

class ChunkImpl
{
protected:
    ChunkInfo    info;

public:
    /**
     * Default constructs.
     */
    ChunkImpl()
        : info{}
    {}

    /**
     * Constructs.
     * @param[in] info  Chunk information
     */
    ChunkImpl(const ChunkInfo& info)
        : info{info}
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
    ProdIndex getProdIndex() const noexcept
    {
        return info.getProdIndex();
    }

    /**
     * Returns the size, in bytes, of the associated product.
     * @return the size, in bytes, of the associated product
     */
    ProdSize getProdSize() const noexcept
    {
        return info.getProdSize();
    }

    /**
     * Returns the size of the data-chunk in bytes.
     * @return Size of the data-chunk in bytes
     */
    ChunkSize getSize() const noexcept
    {
        return info.getSize();
    }
};

/******************************************************************************/

class LatentChunk::Impl final : public ChunkImpl
{
    typedef std::shared_ptr<Impl> SharedPimpl;

    Decoder*     decoder;
    unsigned     version;
    bool         drained;

public:
    inline static SharedPimpl& getEmptySharedPimpl()
    {
        static SharedPimpl pImpl{};     // Empty shared pointer
        return pImpl;
    }

    inline static Impl* getEmptyPimpl()
    {
        static Impl emptyImpl{}; // Default-constructed implementation
        return &emptyImpl;
    }

    /**
     * Constructs from nothing.
     */
    Impl()
        : ChunkImpl()
        , decoder(nullptr)
        , version(0)
        , drained{true}
    {}

    /**
     * Constructs from a serialized representation in a decoder.
     * @param[in] decoder   Decoder. *Must* exist for the duration of this
     *                      instance
     * @param[in] version   Protocol version
     */
    Impl(
            Decoder&       decoder,
            const unsigned version)
        // Keep consistent with ActualChunkImpl::serialize()
        : ChunkImpl(ChunkInfo::deserialize(decoder, version))
        , decoder(&decoder)
        , version(version)
        , drained{false}
    {}

    /**
     * Destroys. Ensures that any and all data no longer exists.
     */
    ~Impl()
    {
        discard();
    }

    /**
     * Returns the byte-offset of the chunk-of-data.
     * @return Byte-offset of the chunk-of-data
     */
    ChunkOffset getOffset() const noexcept
    {
        return info.getOffset();
    }

    /**
     * Returns the index of the chunk-of-data.
     * @return Index of the chunk-of-data
     */
    ChunkIndex getIndex() const noexcept
    {
        return info.getIndex();
    }

    /**
     * Drains the chunk of data into a buffer. The latent data will no longer
     * be available.
     * @param[in] data            Buffer to drain the chunk of data into
     * @param[in] size            Size of the buffer in bytes
     * @return                    Number of bytes actually transferred
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    size_t drainData(
            void* const  data,
            const size_t size)
    {
        if (drained)
            throw LOGIC_ERROR("Latent chunk-of-data already drained");
        const size_t nbytes = decoder->decode(data, size);
        drained = true;
        return nbytes;
    }

    /**
     * Discards the chunk of data. The latent data will no longer be available.
     * Idempotent.
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void discard()
    {
        if (!drained) {
            decoder->clear();
            drained = true;
        }
    }

    /**
     * Indicates if this instance has data (i.e., whether or not `drainData()`
     * has been called).
     * @retval true   This instance has data
     * @retval false  This instance doesn't have data
     */
    bool hasData()
    {
        return !drained;
    }
};

LatentChunk::LatentChunk()
    : pImpl{Impl::getEmptySharedPimpl(), Impl::getEmptyPimpl()}
{}

LatentChunk::LatentChunk(
        Decoder&       decoder,
        const unsigned version)
    : pImpl{new Impl(decoder, version)}
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

ProdIndex LatentChunk::getProdIndex() const noexcept
{
    return pImpl->getProdIndex();
}

ProdSize LatentChunk::getProdSize() const noexcept
{
    return pImpl->getProdSize();
}

ChunkOffset LatentChunk::getOffset() const noexcept
{
    return pImpl->getOffset();
}

ChunkIndex LatentChunk::getIndex() const noexcept
{
    return pImpl->getIndex();
}

ChunkSize LatentChunk::getSize() const noexcept
{
    return pImpl->getSize();
}

size_t LatentChunk::drainData(
        void* const  data,
        const size_t size)
{
    return pImpl->drainData(data, size);
}

void LatentChunk::discard()
{
    pImpl->discard();
}

bool LatentChunk::hasData()
{
    return pImpl->hasData();
}

/******************************************************************************/

class ActualChunk::Impl final : public ChunkImpl
{
    const void* data;

public:
    /**
     * Constructs from nothing.
     */
    Impl()
        : ChunkImpl()
        , data(nullptr)
    {}

    /**
     * Constructs from information on the chunk and a pointer to its data.
     * @param[in] info  Chunk information
     * @param[in] data  Chunk data. Must exist for the duration of the
     *                  constructed instance.
     */
    Impl(
            const ChunkInfo& info,
            const void*      data)
        : ChunkImpl{info}
        , data{data}
    {}

    /**
     * Returns the byte-offset of the chunk-of-data.
     * @return Byte-offset of the chunk-of-data
     */
    ChunkOffset getChunkOffset() const noexcept
    {
        return info.getOffset();
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
     * Returns the number of bytes in the serialized representation.
     * @param[in] version  Protocol version
     * @return Number of bytes in serial representation
     * @exceptionsafety No throw
     * @threadsafety    Safe
     */
    size_t getSerialSize(const unsigned version) const noexcept
    {
        // Keep consistent with `serialize()`
        return info.getSerialSize(version) +
                Codec::getSerialSize(info.getSize());
    }

    /**
     * Serializes this instance to an encoder.
     * @param[in] encoder   Encoder
     * @param[in] version   Protocol version
     * @return Number of bytes written
     * @exceptionsafety Basic
     * @threadsafety Compatible but not safe
     */
    size_t serialize(
            Encoder&       encoder,
            const unsigned version)
    {
        /*
         * Keep consistent with `LatentChunkImpl::LatentChunkImpl(Decoder,
         * unsigned)`
         */
        return info.serialize(encoder, version) +
                encoder.encode(data, info.getSize());
    }

    /**
     * Indicates if this instance is equal to another instance.
     * @param[in] that  Other instance
     * @retval `true`   Instances are equal
     * @retval `false`  Instances are not equal
     */
    bool operator ==(const ActualChunk::Impl& that) const noexcept
    {
        return (info == that.info) &&
                !::memcmp(data, that.data, info.getSize());
    }
};

ActualChunk::ActualChunk()
    : pImpl(new Impl())
{}

ActualChunk::ActualChunk(
        const ChunkInfo& info,
        const void*      data)
    : pImpl{new Impl(info, data)}
{}

const ChunkInfo& ActualChunk::getInfo() const noexcept
{
    return pImpl->getInfo();
}

ProdIndex ActualChunk::getProdIndex() const noexcept
{
    return pImpl->getProdIndex();
}

ChunkIndex ActualChunk::getOffset() const noexcept
{
    return pImpl->getChunkOffset();
}

ChunkSize ActualChunk::getSize() const noexcept
{
    return pImpl->getSize();
}

const void* ActualChunk::getData() const noexcept
{
    return pImpl->getData();
}

size_t ActualChunk::getSerialSize(const unsigned version) const noexcept
{
    return pImpl->getSerialSize(version);
}

size_t ActualChunk::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    return pImpl->serialize(encoder, version);
}

bool ActualChunk::operator ==(const ActualChunk& that) const noexcept
{
    return *pImpl == *that.pImpl;
}

} // namespace
