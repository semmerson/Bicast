/**
 * This file declares two types of chunks of data: 1) a latent chunk that must
 * be read from an object channel; and 2) a reified chunk with a pointer to its
 * data. The two types are in the same file to support keeping their
 * serialization and de-serialization methods consistent.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Chunk.h
 * @author: Steven R. Emmerson
 */

#ifndef CHUNK_H_
#define CHUNK_H_

#include "Codec.h"
#include "error.h"
#include "ProdIndex.h"
#include "ProdSize.h"
#include "SctpSock.h"
#include "SerialInt.h"
#include "Serializable.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <functional>
#include <memory>

namespace hycast {

class ProdInfo;

/******************************************************************************/

class ChunkSize final : public Serializable<ChunkSize>
{
public:
    typedef uint16_t type;

private:
    type             size;

public:
    static const type maxSize     = UINT16_MAX;
    static const type defaultSize = INT16_MAX;

    /**
     * Constructs. NB: Not explicit.
     * @param[in] size  Size of a chunk in bytes
     */
    inline ChunkSize(const type size = 0) noexcept
        : size{size}
    {}

    /**
     * Converts.
     */
    inline operator type() const noexcept
    {
        return size;
    }

    inline std::string to_string() const
    {
        return std::to_string(size);
    }

    /**
     * Returns the hash code of this instance.
     * @return          This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    inline size_t hash() const noexcept {
        return std::hash<type>{}(size);
    }

    inline bool operator ==(const ChunkSize& that) const noexcept {
        return size == that.size;
    }
    inline bool operator !=(const ChunkSize& that) const noexcept {
        return size != that.size;
    }

    /**
     * Returns the number of bytes in the serial representation of an
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    static size_t getStaticSerialSize(const unsigned version) noexcept {
        return Codec::getSerialSize(sizeof(type));
    }

    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    inline size_t getSerialSize(unsigned version) const noexcept {
        return getStaticSerialSize(version);
    }

    inline size_t serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        return encoder.encode(size);
    }

    inline static ChunkSize deserialize(
            Decoder&       decoder,
            const unsigned version)
    {
        type size;
        decoder.decode(size);
        return size;
    }
};

/******************************************************************************/

class ChunkOffset final : public Serializable<ChunkOffset>
{
public:
    typedef uint32_t type;

private:
    SerialInt<type> offset;

public:
    static const type chunkOffsetMax = UINT32_MAX;

    /**
     * Constructs. NB: Not explicit.
     * @param[in] offset  Product offset
     */
    inline ChunkOffset(const type offset = 0) noexcept
        : offset{offset}
    {}

    operator type() const noexcept
    {
        return offset;
    }

    int64_t operator +(const int rhs) const noexcept
    {
        return offset + rhs;
    }

    inline std::string to_string() const
    {
        return offset.to_string();
    }

    /**
     * Returns the hash code of this instance.
     * @return          This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    inline size_t hash() const noexcept {
        return offset.hash();
    }

    inline bool operator ==(const ChunkOffset& that) const noexcept {
        return offset == that.offset;
    }
    inline bool operator !=(const ChunkOffset& that) const noexcept {
        return offset != that.offset;
    }
    inline bool operator <(const ChunkOffset& that) const noexcept {
        return offset < that.offset;
    }
    inline bool operator <=(const ChunkOffset& that) const noexcept {
        return offset <= that.offset;
    }
    inline bool operator >(const ChunkOffset& that) const noexcept {
        return offset > that.offset;
    }
    inline bool operator >=(const ChunkOffset& that) const noexcept {
        return offset >= that.offset;
    }

    /**
     * Returns the number of bytes in the serial representation of an
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    static size_t getStaticSerialSize(const unsigned version) noexcept {
        return Codec::getSerialSize(sizeof(type));
    }

    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    inline size_t getSerialSize(unsigned version) const noexcept {
        return offset.getSerialSize(version);
    }

    inline size_t serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        return offset.serialize(encoder, version);
    }

    inline static ChunkOffset deserialize(
            Decoder&       decoder,
            const unsigned version)
    {
        return SerialInt<type>::deserialize(decoder, version);
    }
};

/******************************************************************************/

class ChunkIndex : public Serializable<ChunkIndex>
{
public:
    typedef ProdSize::type type;

private:
    type                   index;

public:
    ChunkIndex()
        : index{0}
    {}

    inline ChunkIndex(const type index)
        : index{index}
    {}

    inline operator type() const noexcept
    {
        return index;
    }

    inline ChunkIndex& operator ++() noexcept
    {
        ++index;
        return *this;
    }

    inline bool operator <(const ChunkIndex& rhs) const noexcept
    {
        return index < rhs.index;
    }

    /**
     * Returns the number of bytes in the serial representation of any
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    static size_t getStaticSerialSize(const unsigned version) noexcept {
        return Codec::getSerialSize(sizeof(type));
    }

    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    inline size_t getSerialSize(unsigned version) const noexcept {
        return getStaticSerialSize(version);
    }

    inline size_t serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        return encoder.encode(index);
    }

    inline static ChunkIndex deserialize(
            Decoder&       decoder,
            const unsigned version)
    {
        return SerialInt<type>::deserialize(decoder, version);
    }

    inline std::string to_string() const
    {
        return std::to_string(index);
    }
}; // `ChunkIndex`

/******************************************************************************/

class ChunkInfo;

class ChunkId final : public Serializable<ChunkId>
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    ChunkId() noexcept;

    /**
     * Constructs.
     * @param[in] decoder  Decoding object
     * @param[in] version  Protocol version
     */
    ChunkId(Decoder& decoder,
            unsigned version);

    /**
     * Constructs.
     * @param[in] prodInfo     Product information
     * @param[in] chunkIndex   Chunk index
     * @throw InvalidArgument  Product information and chunk index are
     *                         incompatible
     */
    ChunkId(const ProdInfo&  prodInfo,
            const ChunkIndex index);

    /**
     * Constructs.
     * @param[in] info  Chunk information
     */
    explicit ChunkId(const ChunkInfo& info);

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval `true`   Instance is valid
     * @retval `false`  Instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Returns the chunk-index of this instance.
     * @return              Chunk-index of this instance
     */
    ChunkIndex getChunkIndex() const;

    /**
     * Indicates if this instance is earlier than another.
     * @param[in] that   Other instance
     * @retval `true`    Yes
     * @retval `false`   No
     */
    bool isEarlierThan(const ChunkId& that) const noexcept;

    /**
     * Returns the product index.
     * @return the product index
     */
    ProdIndex getProdIndex() const noexcept;

    /**
     * Indicates if this instance is considered equal to another.
     * @param[in] that  Other instance
     * @retval `true`   This instance equals the other
     * @retval `false`  This instance doesn't equal the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator==(const ChunkId& that) const noexcept;

    /**
     * Returns the hash-code of this instance.
     * @return This instance's hash-code
     * @execeptionsafety Nothrow
     * @threadsafety     Safe
     */
    size_t hash() const noexcept;

    /**
     * Indicates if this instance is considered less than another.
     * @param[in] that  Other instance
     * @return `true`   This instance is considered less than the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator<(const ChunkId& that) const noexcept;

    /**
     * Returns the size of a serialized instance in bytes.
     * @param[in] version  Protocol version
     * @return the size of a serialized instance in bytes
     */
    static size_t getStaticSerialSize(const unsigned version) noexcept;

    /**
     * Returns the size, in bytes, of the serialized representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return             Serialized size, in bytes, of this instance
     */
    size_t getSerialSize(unsigned version) const noexcept;

    /**
     * Serializes this instance to an encoder.
     * @param[out] encoder  Encoder
     * @param[in]  version  Protocol version
     * @return Number of bytes written
     */
    size_t serialize(
            Encoder&       encoder,
            const unsigned version) const;

    /**
     * Returns a new instance corresponding to a serialized representation in a
     * decoder.
     * @param[in]  decoder  Decoder
     * @param[in]  version  Protocol version
     * @exceptionsafety     Basic
     * @threadsafety        Compatible but not thread-safe
     */
    static ChunkId deserialize(
            Decoder&          decoder,
            const unsigned    version);

    /**
     * Returns a string representation of this instance.
     * @return String representation
     */
    std::string to_string() const;
}; // ChunkId

/******************************************************************************/

class ChunkInfo
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    ChunkInfo();

    /**
     * Constructs.
     * @param[in] prodInfo     Product information
     * @param[in] chunkIndex   Chunk index
     * @throw InvalidArgument  Product information and chunk index are
     *                         inconsistent
     */
    ChunkInfo(
            const ProdInfo&  prodInfo,
            const ChunkIndex chunkIndex);

    /**
     * Returns the product-information. The product name will be empty.
     * @return Product-information
     */
    const ProdInfo& getProdInfo() const noexcept;

    /**
     * Indicates if this instance is valid (i.e., wasn't default-constructed).
     * @retval `true`    Instance is valid
     * @retval `false`   Instance is not valid
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    operator bool() const noexcept;

    /**
     * Returns the chunk identifier.
     * @return Chunk identifier
     */
    ChunkId getId() const;

    /**
     * Returns the product index.
     * @return Product index
     */
    ProdIndex getProdIndex() const noexcept;

    /**
     * Returns the product size.
     * @return Product size
     */
    ProdSize getProdSize() const noexcept;

    /**
     * Returns the size of a canonical data-chunk.
     * @return Size of canonical data-chunk
     */
    ChunkSize getCanonSize() const noexcept;

    /**
     * Returns the size of the data in this instance.
     * @return Size of data
     */
    ChunkSize getSize() const noexcept;

    /**
     * Returns the chunk offset.
     * @return Chunk offset
     */
    ChunkOffset getOffset() const noexcept;

    /**
     * Returns the index of the data-chunk.
     * @return Chunk index
     */
    ChunkIndex getIndex() const noexcept;

    /**
     * Indicates if this instance is considered equal to another except for,
     * perhaps, the product-name.
     * @param[in] that  Other instance
     * @retval `true`   Instances are equal
     * @retval `false`  Instances are not equal
     */
    bool equalsExceptName(const ChunkInfo& that) const noexcept;

    /**
     * Indicates if this instance is considered equal to another.
     * @param[in] that  Other instance
     * @retval `true`   This instance equals the other
     * @retval `false`  This instance doesn't equal the other
     */
    bool operator==(const ChunkInfo& that) const noexcept;

    /**
     * Returns a string representation of this instance.
     * @return           String representation
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    std::string to_string() const;
}; // ChunkInfo

} // namespace
namespace std {
    inline string to_string(const hycast::ChunkInfo& info)
    {
        return info.to_string();
    }
} // namespace
namespace hycast {

/******************************************************************************/

/**
 * Abstract base class for a chunk of data.
 */
class BaseChunk
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    BaseChunk(Impl* impl);

public:
    /**
     * Default constructs.
     */
    BaseChunk();

    /**
     * Constructs.
     * @param[in] chunkId         Chunk identifier
     * @param[in] prodSize        Size of the associated data-product
     * @param[in] chunkSize       Size of this data-chunk
     * @param[in] canonChunkSize  Size of a canonical data-chunk
     */
    BaseChunk(
            const ChunkId&  chunkId,
            const ProdSize  prodSize,
            const ChunkSize chunkSize,
            const ChunkSize canonChunkSize);

    /**
     * Destroys.
     */
    virtual ~BaseChunk() =0;

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval `true`   Instance is valid
     * @retval `false`  Instance is not valid
     */
    inline operator bool()
    {
        return pImpl.operator bool();
    }

    /**
     * Returns information on the data-chunk.
     * @return Data-chunk information
     */
    ChunkInfo getInfo() const noexcept;

    /**
     * Returns the chunk identifier.
     * @return           Chunk identifier
     * @exceptionsafety  Strong
     * @threadsafety     Safe
     */
    const ChunkId getId() const noexcept;

    /**
     * Returns the index of the associated product.
     * @return the index of the associated product
     */
    ProdIndex getProdIndex() const noexcept;

    /**
     * Returns the size of the associated product.
     * @return Size of associated product
     */
    ProdSize getProdSize() const noexcept;

    /**
     * Returns the size of a canonical data-chunk.
     * @return Size of canonical data-chunk
     */
    ChunkSize getCanonSize() const noexcept;

    /**
     * Returns the byte-offset of the data-chunk in the data-product's data.
     * @return Byte-offset of data-chunk
     */
    ChunkOffset getOffset() const noexcept;

    /**
     * Returns the size of the data-chunk.
     * @return Size of data-chunk
     */
    ChunkSize getSize() const noexcept;

    /**
     * Returns the origin-0 index of this instance.
     * @return Origin-0 index
     */
    ChunkIndex getIndex() const noexcept;

    /**
     * Returns the number of bytes in the metadata of a serialized
     * representation (i.e., *excluding* the actual bytes of data).
     * @param[in] version  Protocol version
     * @return             Number of bytes in metadata
     * @exceptionsafety    No throw
     * @threadsafety       Safe
     */
    static size_t getMetadataSize(const unsigned version) noexcept;

    /**
     * Returns the number of bytes in the serialized representation of this
     * instance -- including the actual bytes of data.
     * @param[in] version  Protocol version
     * @return             Number of bytes in metadata
     * @exceptionsafety    No throw
     * @threadsafety       Safe
     */
    size_t getSerialSize(const unsigned version) const noexcept;
}; // BaseChunk

/******************************************************************************/

/**
 * A memory-based data-chunk.
 */
class ActualChunk final : public BaseChunk
{
    class Impl;

public:
    /**
     * Constructs from nothing.
     */
    ActualChunk();

    /**
     * Constructs.
     * @param[in] prodInfo  Product information
     * @param[in] chunkId   Chunk identifier
     * @param[in] data      Chunk data. *Must* exist for duration of constructed
     *                      instance.
     */
    ActualChunk(
            const ProdInfo&  prodInfo,
            const ChunkIndex chunkIndex,
            const void*      data);

    /**
     * Constructs.
     * @param[in] chunkInfo  Data-chunk information
     * @param[in] data       Chunk data. *Must* exist for duration of
     *                       constructed instance.
     */
    ActualChunk(
            const ChunkInfo& chunkInfo,
            const void*      data);

    /**
     * Returns a pointer to the data.
     * @returns a pointer to the data
     * @exceptionsafety Nothrow
     * @threadsafety Safe
     */
    const void* getData() const noexcept;

    /**
     * Serializes this instance to an encoder.
     * @param[in] encoder  Encoder
     * @param[in] version  Protocol version
     * @return Number of bytes written
     * @exceptionsafety Basic
     * @threadsafety Compatible but not safe
     */
    size_t serialize(
            Encoder&       encoder,
            const unsigned version) const;

    /**
     * Indicates if this instance is considered equal to another.
     * @param[in] rhs   Other instance
     * @retval `true`   Instances are equal
     * @retval `false`  Instances are not equal
     */
    bool operator==(const ActualChunk& rhs) const;
}; // `ActualChunk`

/******************************************************************************/

/**
 * A chunk of data that must be read from a decoder.
 */
class LatentChunk final : public BaseChunk
{
    class                 Impl;

public:
    /**
     * Constructs from nothing.
     */
    LatentChunk();

    /**
     * Constructs from a decoder.
     * @param[in] decoder   Decoder
     * @param[in] version   Protocol version
     * @throws std::invalid_argument if the current record is invalid
     */
    LatentChunk(
            Decoder&       decoder,
            const unsigned version);

    static LatentChunk deserialize(
            Decoder&       decoder,
            const unsigned version);

    /**
     * Drains the chunk of data into a buffer. The latent data will no longer
     * be available.
     * @param[in] data  Buffer to drain the chunk of data into
     * @param[in] size  Size of buffer in bytes
     * @return Number of bytes actually transferred
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    size_t drainData(
            void* const  data,
            const size_t size);

    /**
     * Discards the chunk of data. The latent data will no longer be available.
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    void discard() const;

    /**
     * Indicates if this instance has data (i.e., whether or not `drainData()`
     * has been called).
     * @retval true   This instance has data
     * @retval false  This instance doesn't have data
     */
    bool hasData();

    /**
     * Indicates if this instance is considered equal to another.
     * @param[in] that  Other instance
     * @retval `true`   Instances are equal
     * @retval `false`  Instances are not equal
     */
    bool operator ==(const LatentChunk& that) const noexcept;
};

} // namespace

/******************************************************************************/

namespace std {
    template<> struct hash<hycast::ChunkSize> {
        size_t operator()(const hycast::ChunkSize& size) const noexcept {
            return size.hash();
        }
    };

    template<> struct less<hycast::ChunkSize> {
        bool operator()(const hycast::ChunkSize& size1,
                const hycast::ChunkSize& size2) const noexcept {
            return size1 < size2;
        }
    };

    template<> struct equal_to<hycast::ChunkSize> {
        bool operator()(const hycast::ChunkSize& size1,
                const hycast::ChunkSize& size2) const noexcept {
            return size1 == size2;
        }
    };

    template<> struct hash<hycast::ChunkOffset> {
        size_t operator()(const hycast::ChunkOffset& offset) const noexcept {
            return offset.hash();
        }
    };

    template<> struct less<hycast::ChunkOffset> {
        bool operator()(const hycast::ChunkOffset& offset1,
                const hycast::ChunkOffset& offset2) const noexcept {
            return offset1 < offset2;
        }
    };

    template<> struct equal_to<hycast::ChunkOffset> {
        bool operator()(const hycast::ChunkOffset& offset1,
                const hycast::ChunkOffset& offset2) const noexcept {
            return offset1 == offset2;
        }
    };

    template<> struct hash<hycast::ChunkId> {
        size_t operator()(const hycast::ChunkId& chunkId) const noexcept {
            return chunkId.hash();
        }
    };

    template<> struct less<hycast::ChunkId> {
        bool operator()(const hycast::ChunkId& chunkId1,
                const hycast::ChunkId& chunkId2) const noexcept {
            return chunkId1 < chunkId2;
        }
    };

    template<> struct equal_to<hycast::ChunkId> {
        bool operator()(const hycast::ChunkId& chunkId1,
                const hycast::ChunkId& chunkId2) const noexcept {
            return chunkId1 == chunkId2;
        }
    };

    inline string to_string(hycast::ChunkIndex index) {
        return std::to_string(static_cast<hycast::ChunkIndex::type>(index));
    }
}

#endif /* CHUNK_H_ */
