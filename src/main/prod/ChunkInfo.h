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

#include <cstddef>

namespace hycast {

class ChunkInfo final : public Serializable {
    ProdIndex  prodIndex;
    ChunkIndex chunkIndex;
public:
    /**
     * Constructs from nothing.
     */
    ChunkInfo()
        : prodIndex(0),
          chunkIndex(0) {}
    /**
     * Constructs from product and chunk indexes.
     * @param[in] prodIndex   Product index
     * @param[in] chunkIndex  Chunk index
     */
    ChunkInfo(
            const ProdIndex  prodIndex,
            const ChunkIndex chunkIndex)
        : prodIndex(prodIndex),
          chunkIndex(chunkIndex) {}
    /**
     * Constructs by de-serializing from a buffer.
     * @param[in] buf      Buffer
     * @param[in] size     Size of buffer in bytes
     * @param[in] version  Protocol version
     */
    ChunkInfo(
            const char*    buf,
            const size_t   size,
            const unsigned version);
    /**
     * Returns the size of a serialized instance in bytes.
     * @param[in] version  Protocol version
     * @return the size of a serialized instance in bytes
     */
    size_t getSerialSize(unsigned version) const noexcept {
        return prodIndex.getSerialSize(version) + sizeof(chunkIndex);
    }
    /**
     * Returns the product index.
     * @return the product index
     */
    ProdIndex getProdIndex() const {return prodIndex;}
    /**
     * Returns the chunk index.
     * @return the chunk index
     */
    ChunkIndex getChunkIndex() const {return chunkIndex;}
    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @retval `true` iff this instance equals the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator==(const ChunkInfo& that) const noexcept;
    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
     * @execeptionsafety Nothrow
     * @threadsafety     Safe
     */
    size_t hash() const noexcept {
        return prodIndex.hash() | std::hash<ChunkIndex>()(chunkIndex);
    }
    /**
     * Indicates if this instance is less than another.
     * @param[in] that  Other instance
     * @return `true` iff this instance is less than the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator<(const ChunkInfo& that) const noexcept {
        return prodIndex < that.prodIndex ||
                (prodIndex == that.prodIndex && chunkIndex < that.chunkIndex);
    }
    /**
     * Serializes this instance to a buffer.
     * @param[out] buf      Buffer
     * @param[in]  size     Size of buffer in bytes
     * @param[in]  version  Protocol version
     * @return Address of next byte
     */
    char* serialize(
            char*          buf,
            const size_t   nbytes,
            const unsigned version) const;
    /**
     * Returns a new instance corresponding to a serialized representation in a
     * buffer.
     * @param[in] buf      Buffer
     * @param[in] size     Size of buffer in bytes
     * @param[in] version  Protocol version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    static ChunkInfo deserialize(
            const char* const buf,
            const size_t      size,
            const unsigned    version);
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
