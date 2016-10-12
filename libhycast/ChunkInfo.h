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
#include "Serializable.h"

#include <istream>

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
            const void*    buf,
            const size_t   size,
            const unsigned version);
    /**
     * Constructs by de-serializing from an input stream.
     * @param[in] istream  Input stream
     * @param[in] version  Protocol version
     */
    ChunkInfo(
            std::istream&  istream,
            const unsigned version);
    /**
     * Returns the size of a serialized instance in bytes.
     * @param[in] version  Protocol version
     * @return the size of a serialized instance in bytes
     */
    size_t getSerialSize(unsigned version) const {
        return sizeof(prodIndex) + sizeof(chunkIndex);
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
     */
    bool equals(const ChunkInfo& that) const;
    /**
     * Serializes this instance to a buffer.
     * @param[out] buf      Buffer
     * @param[in]  size     Size of buffer in bytes
     * @param[in]  version  Protocol version
     */
    void serialize(
            void*          buf,
            const size_t   nbytes,
            const unsigned version) const;
};

} // namespace

#endif /* CHUNKINFO_H_ */
