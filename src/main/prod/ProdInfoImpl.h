/**
 * This file declares an implementation of information about a product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfoImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef PRODINFOIMPL_H_
#define PRODINFOIMPL_H_

#include "Chunk.h"
#include "HycastTypes.h"
#include "ProdIndex.h"
#include "Serializable.h"

#include <cstdint>
#include <memory>
#include <string>

namespace hycast {

class ProdInfoImpl : public Serializable {
    std::string name;
    ProdIndex   index;
    ProdSize    size;
    ChunkSize   chunkSize;
public:
    /**
     * Constructs from information on a product.
     * @param[in] name       Product name
     * @param[in] index      Product index
     * @param[in] size       Size of product in bytes
     * @param[in] chunkSize  Size of data chunks in bytes
     * @throws std::invalid_argument if `name.size() > prodNameSizeMax`
     */
    ProdInfoImpl(
            const std::string& name,
            const ProdIndex    index,
            const ProdSize     size,
            const ChunkSize    chunkSize);
    /**
     * Constructs by deserializing a serialized representation from a buffer.
     * @param[in] buf      Buffer
     * @param[in] version  Serialization version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    ProdInfoImpl(
            const char* const buf,
            const size_t      size,
            const unsigned    version);
    /**
     * Returns the name of the product.
     * @return Name of the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const std::string& getName() const {return name;}
    /**
     * Returns the index of the product.
     * @return index of the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ProdIndex getIndex() const {return index;}
    /**
     * Returns the size of the product in bytes.
     * @return Size of the product in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ProdSize getSize() const {return size;}
    /**
     * Returns the size of the product's data chunks in bytes.
     * @return Size of the product's data chunks in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkSize getChunkSize() const {return chunkSize;}
    /**
     * Returns the size, in bytes, of a given chunk-of-data.
     * @param[in] index  Index of the chunk
     * @return           The size of the chunk in bytes
     * @throws std::invalid_argument if the index is invalid
     * @execeptionsafety Strong guarantee
     * @threadsafety     Safe
     */
    ChunkSize getChunkSize(ChunkIndex index) const;
    /**
     * Returns the number of chunks in the product.
     * @return the number of chunks in the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkIndex getNumChunks() const { return (size+chunkSize-1)/chunkSize; }
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
            const ChunkSize  chunkSize) const;
    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance
     * @retval true   This instance is equal to the other
     * @retval false  This instance is not equal to the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator==(const ProdInfoImpl& that) const noexcept;
    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    size_t getSerialSize(unsigned version) const noexcept;
    /**
     * Serializes this instance to a buffer.
     * @param[in] buf       Buffer
     * @param[in] size      Buffer size in bytes
     * @param[in] version   Serialization version
     * @return Address of next byte
     * @execptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    char* serialize(
            char*          buf,
            const size_t   size,
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
    static std::shared_ptr<ProdInfoImpl> deserialize(
            const char* const buf,
            const size_t      size,
            const unsigned    version);
};

} // namespace

#endif /* PRODINFOIMPL_H_ */
