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
#include "Codec.h"
#include "error.h"
#include "ProdInfo.h"

#include <cstring>

namespace hycast {

class ChunkId::Impl
{
    /**
     * Index of the associated data-product.
     */
    ProdIndex                   prodIndex;
    /**
     * Index of the chunk of data in the product's data.
     */
    ChunkIndex                  chunkIndex;
    /**
     * Hash-code of this instance.
     */
    mutable std::atomic<size_t> hashCode;
    /**
     * Whether or not this instance is valid.
     */
    bool                        isValid;

public:
    Impl() noexcept
        : prodIndex{0}
        , chunkIndex{0}
        , hashCode{0}
        , isValid{false}
    {}

    Impl(   Decoder& decoder,
            unsigned version)
        /*
         * Keep consistent with
         * - `serialize(Encoder&, unsigned)`
         * - `getStaticSerialSize(unsigned)`
         */
        : prodIndex{ProdIndex::deserialize(decoder, version)}
        , chunkIndex{ChunkIndex::deserialize(decoder, version)}
        , hashCode{0}
        , isValid{false}
    {
        decoder.decode(isValid);
    }

    Impl(   const ProdInfo&  prodInfo,
            const ChunkIndex chunkIndex)
        : prodIndex{prodInfo.getIndex()}
        , chunkIndex{chunkIndex}
        , hashCode{0}
        , isValid{prodInfo && chunkIndex}
    {}

    Impl(const ChunkInfo& info)
        : prodIndex{info.getProdIndex()}
        , chunkIndex{info.getIndex()}
        , hashCode{0}
        , isValid{info}
    {}

    operator bool() const noexcept
    {
        return isValid;
    }

    ChunkIndex getChunkIndex() const noexcept
    {
        return chunkIndex;
    }

    bool isEarlierThan(const Impl& rhs) const noexcept
    {
        return prodIndex.isEarlierThan(rhs.prodIndex) ||
                (prodIndex == rhs.prodIndex && chunkIndex < rhs.chunkIndex);
    }

    ProdIndex getProdIndex() const noexcept
    {
        return prodIndex;
    }

    bool operator==(const Impl& that) const noexcept
    {
        return prodIndex == that.prodIndex && chunkIndex == that.chunkIndex;
    }

    size_t hash() const noexcept
    {
        if (hashCode.load() == 0)
            hashCode = prodIndex.hash() |
                    std::hash<ChunkIndex::type>()(chunkIndex);
        return hashCode.load();
    }

    bool operator<(const Impl& that) const noexcept
    {
        return (prodIndex < that.prodIndex)
            ? true
            : (prodIndex > that.prodIndex)
              ? false
                : (chunkIndex < that.chunkIndex);
    }

    static size_t getStaticSerialSize(const unsigned version) noexcept
    {
        /*
         * Keep consistent with
         * - `Impl(Decoder&, unsigned)`
         * - `serialize()`
         */
        bool value;
        return ProdIndex::getStaticSerialSize(version) +
                ChunkIndex::getStaticSerialSize(version) +
                Codec::getSerialSize(&value);
    }

    size_t serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        /*
         * Keep consistent with
         * - `ChunkId::ChunkId(Decoder, unsigned)`
         * - `ChunkId::getStaticSerialSize(const unsigned)`
         */
        return prodIndex.serialize(encoder, version) +
                chunkIndex.serialize(encoder, version) +
                encoder.encode(isValid);
    }

    std::string to_string() const
    {
        return "{prodIndex=" + std::to_string(prodIndex) + ", chunkIndex=" +
                std::to_string(chunkIndex) + "}";
    }
}; // `ChunkId::Impl`


ChunkId::ChunkId() noexcept
    : pImpl{new Impl()}
{}

ChunkId::ChunkId(
        Decoder& decoder,
        unsigned version)
    : pImpl{new Impl(decoder, version)}
{}

ChunkId::ChunkId(
        const ProdInfo&  prodInfo,
        const ChunkIndex index)
    : pImpl{new Impl(prodInfo, index)}
{}

ChunkId::ChunkId(
        const ChunkInfo& info)
    : pImpl{new Impl(info)}
{}

ChunkId::operator bool() const noexcept
{
    return pImpl->operator bool();
}

ChunkIndex ChunkId::getChunkIndex() const
{
    return pImpl->getChunkIndex();
}

bool ChunkId::isEarlierThan(const ChunkId& rhs) const noexcept
{
    return pImpl->isEarlierThan(*rhs.pImpl.get());
}

ProdIndex ChunkId::getProdIndex() const noexcept
{
    return pImpl->getProdIndex();
}

bool ChunkId::operator==(const ChunkId& that) const noexcept
{
    return pImpl->operator==(*that.pImpl.get());
}

size_t ChunkId::hash() const noexcept
{
    return pImpl->hash();
}

bool ChunkId::operator<(const ChunkId& that) const noexcept
{
    return pImpl->operator<(*that.pImpl.get());
}

size_t ChunkId::getStaticSerialSize(const unsigned version) noexcept
{
    return Impl::getStaticSerialSize(version);
}

size_t ChunkId::getSerialSize(const unsigned version) const noexcept
{
    return Impl::getStaticSerialSize(version);
}

size_t ChunkId::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    return pImpl->serialize(encoder, version);
}

ChunkId ChunkId::deserialize(
            Decoder&          decoder,
            const unsigned    version)
{
    return ChunkId{decoder, version};
}

std::string ChunkId::to_string() const
{
    return pImpl->to_string();
}

/******************************************************************************/

class ChunkInfo::Impl
{
    ProdInfo   prodInfo;
    ChunkIndex index;

public:
    Impl()
        : prodInfo{}
        , index{}
    {}

    Impl(   const ProdInfo&  prodInfo,
            const ChunkIndex index)
        : prodInfo{prodInfo}
        , index{index}
    {
        prodInfo.vet(index);
    }

    const ProdInfo& getProdInfo() const noexcept
    {
        return prodInfo;
    }

    ProdIndex getProdIndex() const noexcept
    {
        return prodInfo.getIndex();
    }

    ProdSize getProdSize() const noexcept
    {
        return prodInfo.getSize();
    }

    ChunkSize getCanonSize() const noexcept
    {
        return prodInfo.getChunkSize();
    }

    ChunkSize getSize() const noexcept
    {
        // Can't throw because `index` has been vetted by constructor
        return prodInfo.getChunkSize(index);
    }

    ChunkOffset getOffset() const noexcept
    {
        // Can't throw because `index` has been vetted by constructor
        return prodInfo.getChunkOffset(index);
    }

    ChunkIndex getIndex() const noexcept
    {
        return index;
    }

    bool equalsExceptName(const Impl& that)
    {
        return (index == that.index) &&
                prodInfo.equalsExceptName(that.prodInfo);
    }

    bool operator==(const Impl& that)
    {
        return (index == that.index) &&
                (prodInfo.equalsExceptName(that.prodInfo));
    }

    std::string to_string() const
    {
        return "{prodInfo=" + std::to_string(prodInfo) + ", index=" +
                std::to_string(index) + "}";
    }
};

ChunkInfo::ChunkInfo()
    : pImpl{}
{}

ChunkInfo::ChunkInfo(
        const ProdInfo&  prodInfo,
        const ChunkIndex index)
    : pImpl{new Impl(prodInfo, index)}
{}

const ProdInfo& ChunkInfo::getProdInfo() const noexcept
{
    return pImpl->getProdInfo();
}

ChunkInfo::operator bool() const noexcept
{
    return pImpl.operator bool();
}

ChunkId ChunkInfo::getId() const
{
    return ChunkId{*this};
}

ProdIndex ChunkInfo::getProdIndex() const noexcept
{
    return pImpl->getProdIndex();
}

ProdSize ChunkInfo::getProdSize() const noexcept
{
    return pImpl->getProdSize();
}

ChunkSize ChunkInfo::getCanonSize() const noexcept
{
    return pImpl->getCanonSize();
}

ChunkSize ChunkInfo::getSize() const noexcept
{
    return pImpl->getSize();
}

ChunkOffset ChunkInfo::getOffset() const noexcept
{
    return pImpl->getOffset();
}

ChunkIndex ChunkInfo::getIndex() const noexcept
{
    return pImpl->getIndex();
}

bool ChunkInfo::equalsExceptName(const ChunkInfo& that) const noexcept
{
    return pImpl->equalsExceptName(*that.pImpl.get());
}

bool ChunkInfo::operator==(const ChunkInfo& that) const noexcept
{
    return pImpl->operator==(*that.pImpl.get());
}

std::string ChunkInfo::to_string() const
{
    return pImpl->to_string();
}

/******************************************************************************/

class BaseChunk::Impl
{
protected:
    ChunkInfo info;

public:
    /**
     * Default constructs.
     */
    Impl()
        : info{}
    {}

    /**
     * Constructs.
     * @param[in] prodInfo    Product information
     * @param[in] chunkIndex  Chunk index
     */
    Impl(   const ProdInfo&  prodInfo,
            const ChunkIndex chunkIndex)
        : info{prodInfo, chunkIndex}
    {}

    Impl(const ChunkInfo& info)
        : info{info}
    {}

    virtual ~Impl() =default;

    ChunkInfo getInfo() const noexcept
    {
        return info;
    }

    /**
     * Returns the chunk identifier.
     * @return           Chunk identifier
     * @exceptionsafety  Strong
     * @threadsafety     Safe
     */
    ChunkId getId() const noexcept
    {
        return info.getId();
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
     * Returns the size of the associated product.
     * @return Size of associated product
     */
    ProdSize getProdSize() const noexcept
    {
        return info.getProdSize();
    }

    ChunkSize getCanonSize() const noexcept
    {
        return info.getCanonSize();
    }

    /**
     * Returns the offset of this instance.
     * @return Chunk offset
     */
    ChunkOffset getOffset() const noexcept
    {
        return info.getOffset();
    }

    /**
     * Returns the size of the data-chunk.
     * @return Size of data-chunk
     */
    ChunkSize getSize() const noexcept
    {
        return info.getSize();
    }

    static size_t getMetadataSize(const unsigned version) noexcept
    {
        /*
         * Keep consistent with
         * - `ActualChunk::Impl::serialize(Encoder&, unsigned)`
         * - `LatentChunk::Impl::Impl(Decoder&, unsigned)`
         */
        return ChunkId::getStaticSerialSize(version) +
                ProdSize::getStaticSerialSize(version) +
                ChunkSize::getStaticSerialSize(version);
    }

    size_t getSerialSize(const unsigned version) const noexcept
    {
        return getMetadataSize(version) + info.getSize();
    }

    ChunkIndex getIndex() const noexcept
    {
        return info.getIndex();
    }

    bool operator==(const Impl& that) const noexcept
    {
        return info == that.info;
    }
};

BaseChunk::BaseChunk()
    : pImpl{}
{}

BaseChunk::BaseChunk(Impl* impl)
    : pImpl{impl}
{}

BaseChunk::~BaseChunk() =default;

ChunkInfo BaseChunk::getInfo() const noexcept
{
    return pImpl->getInfo();
}

const ChunkId BaseChunk::getId() const noexcept
{
    return pImpl->getId();
}

ProdIndex BaseChunk::getProdIndex() const noexcept
{
    return pImpl->getProdIndex();
}

ProdSize BaseChunk::getProdSize() const noexcept
{
    return pImpl->getProdSize();
}

ChunkSize BaseChunk::getCanonSize() const noexcept
{
    return pImpl->getCanonSize();
}

ChunkOffset BaseChunk::getOffset() const noexcept
{
    return pImpl->getOffset();
}

ChunkSize BaseChunk::getSize() const noexcept
{
    return pImpl->getSize();
}

ChunkIndex BaseChunk::getIndex() const noexcept
{
    return pImpl->getIndex();
}

size_t BaseChunk::getMetadataSize(const unsigned version) noexcept
{
    return Impl::getMetadataSize(version);
}

size_t BaseChunk::getSerialSize(const unsigned version) const noexcept
{
    return pImpl->getSerialSize(version);
}

/******************************************************************************/

class ActualChunk::Impl final : public BaseChunk::Impl
{
    const void* data;

public:
    /**
     * Constructs from nothing.
     */
    Impl()
        : BaseChunk::Impl{}
        , data(nullptr)
    {}

    Impl(   const ProdInfo&    prodInfo,
            const ChunkIndex&  chunkIndex,
            const void*        data)
        : BaseChunk::Impl{prodInfo, chunkIndex}
        , data{data}
    {}

    Impl(   const ChunkInfo&   chunkInfo,
            const void*        data)
        : BaseChunk::Impl{chunkInfo}
        , data{data}
    {}

    Impl(const Impl& impl) =delete;
    Impl(const Impl&& impl) =delete;
    Impl& operator=(const Impl& rhs) =delete;
    Impl& operator=(const Impl&& rhs) =delete;

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
     * @return Number of bytes written
     * @exceptionsafety Basic
     * @threadsafety Compatible but not safe
     */
    size_t serialize(
            Encoder&       encoder,
            const unsigned version)
    {
        /*
         * Keep consistent with
         * - `LatentChunk::Impl::Impl(Decoder&, unsigned)`
         * - `BaseChunk:Impl::getMetadataSize(unsigned)`
         */
        return info.getId().serialize(encoder, version) +
                info.getProdSize().serialize(encoder, version) +
                info.getCanonSize().serialize(encoder, version) +
                encoder.encode(data, info.getSize());
    }

    /**
     * Indicates if this instance is equal to another instance.
     * @param[in] that  Other instance
     * @retval `true`   Instances are equal
     * @retval `false`  Instances are not equal
     */
    bool operator ==(const Impl& that) const noexcept
    {
        return (this == &that) || (
                info.equalsExceptName(that.info) &&
                (::memcmp(data, that.data, info.getSize()) == 0)
        );
    }
};

ActualChunk::ActualChunk()
    : BaseChunk{}
{}

ActualChunk::ActualChunk(
        const ProdInfo&   prodInfo,
        const ChunkIndex  chunkIndex,
        const void*       data)
    : BaseChunk{new Impl(prodInfo, chunkIndex, data)}
{}

ActualChunk::ActualChunk(
        const ChunkInfo&  chunkInfo,
        const void*       data)
    : BaseChunk{new Impl(chunkInfo, data)}
{}

const void* ActualChunk::getData() const noexcept
{
    return static_cast<Impl*>(pImpl.get())->getData();
}

size_t ActualChunk::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    return static_cast<Impl*>(pImpl.get())->serialize(encoder, version);
}

bool ActualChunk::operator==(const ActualChunk& rhs) const
{
    return static_cast<Impl*>(pImpl.get())->
            operator==(*static_cast<Impl*>(rhs.pImpl.get()));
}

/******************************************************************************/

class LatentChunk::Impl final : public BaseChunk::Impl
{
    typedef std::shared_ptr<Impl> SharedPimpl;

    Decoder*     decoder;
    unsigned     version;
    bool         drained;

public:
    /**
     * Constructs from nothing.
     */
    Impl()
        : BaseChunk::Impl()
        , decoder(nullptr)
        , version(0)
        , drained{true}
    {}

    /**
     * Constructs from a serialized representation in a decoder.
     * @param[in] decoder      Decoder. *Must* exist for the duration of this
     *                         instance
     * @param[in] version      Protocol version
     * @throw InvalidArgument  Invalid data-chunk
     */
    Impl(   Decoder&       decoder,
            const unsigned version)
        : BaseChunk::Impl{}
        , decoder(&decoder)
        , version(version)
        , drained{false}
    {
        /*
         * Keep consistent with
         * - `ActualChunk::Impl::serialize(Encoder&, unsigned)`
         * - `BaseChunk:Impl::getMetadataSize(unsigned)`
         */
        auto chunkId = ChunkId::deserialize(decoder, version);
        auto prodSize = ProdSize::deserialize(decoder, version);
        auto canonChunkSize = ChunkSize::deserialize(decoder, version);
        // Can't get size of UDP message
        // Name is empty
        ProdInfo prodInfo{chunkId.getProdIndex(), prodSize, canonChunkSize};
        info = ChunkInfo{prodInfo, chunkId.getChunkIndex()};
    }

    Impl(const Impl& impl) =delete;
    Impl(const Impl&& impl) =delete;
    Impl& operator=(const Impl& rhs) =delete;
    Impl& operator=(const Impl&& rhs) =delete;

    /**
     * Destroys. Ensures that any and all data no longer exists.
     */
    ~Impl()
    {
        discard();
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

    bool operator ==(const Impl& that) const noexcept
    {
        return (this == &that) || (
                (drained == that.drained) &&
                (version == that.version) &&
                (info == that.info) &&
                (decoder == that.decoder));
    }
};

LatentChunk::LatentChunk()
    : BaseChunk{new Impl()}
{}

LatentChunk::LatentChunk(
        Decoder&       decoder,
        const unsigned version)
    : BaseChunk{new Impl(decoder, version)}
{}

LatentChunk LatentChunk::deserialize(
        Decoder&       decoder,
        const unsigned version)
{
    return LatentChunk(decoder, version);
}

size_t LatentChunk::drainData(
        void* const  data,
        const size_t size)
{
    return static_cast<Impl*>(pImpl.get())->drainData(data, size);
}

void LatentChunk::discard() const
{
    static_cast<Impl*>(pImpl.get())->discard();
}

bool LatentChunk::hasData()
{
    return static_cast<Impl*>(pImpl.get())->hasData();
}

bool LatentChunk::operator==(const LatentChunk& that) const noexcept
{
    return pImpl->operator==(*that.pImpl.get());
}

} // namespace
