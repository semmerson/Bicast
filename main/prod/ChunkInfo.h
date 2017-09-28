/**
 * This file declares metadata about a chunk of data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkInfo.h
 * @author: Steven R. Emmerson
 */

#ifndef CHUNKINFO_H_
#define CHUNKINFO_H_

#include "HycastTypes.h"
#include "ProdIndex.h"
#include "Serializable.h"

#include <atomic>
#include <cstddef>
#include <functional>

namespace hycast {

class ProdInfo;

class ChunkInfo final : public Serializable<ChunkInfo> {
    /**
     * Index of the associated data-product.
     */
    ProdIndex                   prodIndex;
    /**
     * The product-size is included in a chunk's information so that the
     * necessary space for a data-product can be completely allocated based on
     * an incoming chunk for which no product-information exists.
     */
    ProdSize                    prodSize;
    /**
     * Origin-0 index of the chunk.
     */
    ChunkIndex                  chunkIndex;
    /**
     * Hash-code of this instance.
     */
    mutable std::atomic<size_t> hashCode;

    /**
     * Constructs.
     * @param[in] prodIndex   Product index
     * @param[in] prodSize    Product size in bytes
     * @param[in] chunkIndex  Chunk index
     */
    ChunkInfo(
            const ProdIndex   prodIndex,
            const ProdSize    prodSize,
            const ChunkIndex  chunkIndex);

public:
    /**
     * Default constructs.
     */
    ChunkInfo() noexcept
        : ChunkInfo(0, 0, 0)
    {}

    /**
     * Copy constructs.
     * @param[in] info  Rvalue chunk information
     */
    ChunkInfo(const ChunkInfo& info);

    /**
     * Constructs.
     * @param[in] prodInfo    Information on associated product
     * @param[in] chunkIndex  Origin-0 chunk index
     * @exceptionsafety       Nothrow
     */
    ChunkInfo(
            const ProdInfo&   prodInfo,
            const ChunkIndex  chunkIndex) noexcept;

    ChunkInfo(
            Decoder& decoder,
            unsigned version);

    /**
     * Copy assigns.
     * @param[in] rhs  Other instance
     * @return         This instance
     */
    const ChunkInfo& operator =(const ChunkInfo& rhs) noexcept;

    /**
     * Indicates if this instance is meaningful or not (i.e. was it default
     * constructed).
     * @retval `true`   Meaningful
     * @retval `false`  Not meaningful
     */
    operator bool() const noexcept;

    /**
     * Indicates if this instance is earlier than another.
     * @param[in] that   Other instance
     * @retval `true`    Yes
     * @retval `false`   No
     */
    bool isEarlierThan(const ChunkInfo& that) const noexcept;

    /**
     * Sets the size of a canonical chunk of data (i.e., all chunks except,
     * perhaps, the last). This should only be done at most once per session.
     * @param[in] size  Size, in bytes, of a canonical chunk-of-data
     */
    static void setCanonSize(const ChunkSize size);

    /**
     * Returns the size of a canonical chunk of data (i.e., all chunks except,
     * perhaps, the last).
     * @return Size of a canonical chunk in bytes
     */
    static ChunkSize getCanonSize();

    /**
     * Returns the size of a chunk of data.
     * @param[in] prodSize    Size of the associated data-product in bytes
     * @param[in] chunkIndex  Origin-0 index of the chunk
     * @return Size of the chunk of data in bytes
     */
    static ChunkSize getSize(
            const ProdSize   prodSize,
            const ChunkIndex chunkIndex);

    /**
     * Returns the product index.
     * @return the product index
     */
    ProdIndex getProdIndex() const {return prodIndex;}

    /**
     * Returns the size of the associated data-product in bytes.
     * @return Size of the associated data-product in bytes
     */
    ProdSize getProdSize() const {return prodSize;}

    /**
     * Returns the offset, in bytes, from the start of the data-product's data
     * to the data of the chunk.
     * @return Offset to chunk's data from start of product's data
     */
    ChunkOffset getOffset() const {return chunkIndex*getCanonSize();}

    /**
     * Returns the offset, in bytes, from the start of the data-product's data
     * to the data of the chunk.
     * @param[in] chunkIndex  Origin-0 index of a chunk of data
     * @return Offset to chunk's data from start of product's data
     */
    static ChunkOffset getOffset(const ChunkIndex chunkIndex)
    {
        return chunkIndex*getCanonSize();
    }

    /**
     * Returns the origin-0 index of the chunk-of-data.
     * @return Origin-0 index of the chunk-of-data
     */
    ChunkIndex getIndex() const noexcept
    {
        return chunkIndex;
    }

    /**
     * Returns the chunk size in bytes.
     * @return the chunk size in bytes
     */
    ChunkSize getSize() const noexcept
    {
        /*
         * An exception can't be thrown because the product size and the chunk
         * index are checked during construction.
         */
        return getSize(prodSize, chunkIndex);
    }

    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @retval `true` iff this instance equals the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator==(const ChunkInfo& that) const noexcept;

    /**
     * Returns the hash-code of this instance.
     * @return This instance's hash-code
     * @execeptionsafety Nothrow
     * @threadsafety     Safe
     */
    size_t hash() const noexcept {
        if (hashCode.load() == 0)
            hashCode = prodIndex.hash() | std::hash<ChunkIndex>()(chunkIndex);
        return hashCode.load();
    }

    /**
     * Indicates if this instance is less than another.
     * @param[in] that  Other instance
     * @return `true` iff this instance is less than the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator<(const ChunkInfo& that) const noexcept {
        return (prodIndex < that.prodIndex)
            ? true
            : (prodIndex > that.prodIndex)
              ? false
                : (chunkIndex < that.chunkIndex)
                  ? true
                  : (chunkIndex > that.chunkIndex)
                    ? false
                    : true;
    }

    /**
     * Returns the size of a serialized instance in bytes.
     * @param[in] version  Protocol version
     * @return the size of a serialized instance in bytes
     */
    static size_t getStaticSerialSize(const unsigned version) noexcept
    {
        // Keep consonant with `serialize()`
        return ProdIndex::getStaticSerialSize(version) +
                Codec::getSerialSize(sizeof(ProdSize)) +
                Codec::getSerialSize(sizeof(chunkIndex));
    }

    /**
     * Returns the size, in bytes, of the serialized representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the serialized size, in bytes, of this instance
     */
    size_t getSerialSize(unsigned version) const noexcept
    {
        return getStaticSerialSize(version);
    }

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
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    static ChunkInfo deserialize(
            Decoder&          decoder,
            const unsigned    version);

    std::string to_string() const;
};

} // namespace

#include <functional>

namespace std {
    template<> struct hash<hycast::ChunkInfo> {
        size_t operator()(const hycast::ChunkInfo& info) const noexcept {
            return info.hash();
        }
    };

    template<> struct less<hycast::ChunkInfo> {
        bool operator()(const hycast::ChunkInfo& info1,
                const hycast::ChunkInfo& info2) const noexcept {
            return info1 < info2;
        }
    };

    template<> struct equal_to<hycast::ChunkInfo> {
        bool operator()(const hycast::ChunkInfo& info1,
                const hycast::ChunkInfo& info2) const noexcept {
            return info1 == info2;
        }
    };
}

#endif /* CHUNKINFO_H_ */
