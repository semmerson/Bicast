/**
 * This file declares information about a product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfo.h
 * @author: Steven R. Emmerson
 */

#ifndef PRODINFO_H_
#define PRODINFO_H_

#include "ChunkInfo.h"
#include "HycastTypes.h"
#include "ProdIndex.h"
#include "Serializable.h"

#include <cstddef>
#include <memory>
#include <string>

namespace hycast {

class ProdInfo : public Serializable<ProdInfo>
{
    class Impl; // Forward declaration

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    ProdInfo();

    /**
     * Constructs from information on a product.
     * @param[in] name       Product name
     * @param[in] index      Product index
     * @param[in] size       Size of product in bytes
     * @throws std::invalid_argument if `name.size() > prodNameSizeMax`
     */
    ProdInfo(
            const std::string& name,
            const ProdIndex    index,
            const ProdSize     size);

    /**
     * Returns a string representation of this instance.
     * @return String representation of this instance
     */
    std::string to_string() const;

    /**
     * Returns the name of the product.
     * @return Name of the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const std::string& getName() const;

    /**
     * Returns the index of the product.
     * @return index of the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ProdIndex getIndex() const noexcept;

    /**
     * Returns the size of the product in bytes.
     * @return Size of the product in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ProdSize getSize() const noexcept;

    /**
     * Indicates if this instance is earlier than another.
     * @param[in] that   Other instance
     * @retval `true`    Yes
     * @retval `false`   No
     */
    bool isEarlierThan(const ProdInfo& that) const noexcept;

    /**
     * Returns the canonical size of the product's data chunks in bytes.
     * @return Canonical size of the product's data chunks in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkSize getChunkSize() const;

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
    ChunkIndex getNumChunks() const;

    /**
     * Returns the offset, in bytes, from the start of the data-product's data
     * to the data of the chunk.
     * @param[in] chunkIndex  Origin-0 index of a chunk of data
     * @return Offset to chunk's data from start of product's data
     * @throws InvalidArgument Chunk-index is greater than or equal to the
     *                         number of chunks
     */
    ChunkOffset getOffset(const ChunkIndex chunkIndex) const;

    /**
     * Vets information on a chunk-of-data ostensibly belonging to this
     * instance's associated product.
     * @param[in] chunkInfo  Information to be vetted
     * @param[in] chunkSize  Size of the chunk in bytes
     * @throws std::invalid_argument if the information is inconsistent with
     *                               this instance's product
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    void vet(const ChunkInfo& chunkInfo,
            const ChunkSize   chunkSize) const;

    /**
     * Returns information on a chunk of data corresponding to a chunk index.
     * @param[in] chunkIndex  Chunk index
     * @return Corresponding chunk information
     * @throws InvalidArgument  The chunk index is invalid
     * @execeptionsafety Strong guarantee
     * @threadsafety     Safe
     */
    ChunkInfo makeChunkInfo(const ChunkIndex chunkIndex) const;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance
     * @retval true   This instance is equal to the other
     * @retval false  This instance is not equal to the other
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool operator==(const ProdInfo& that) const noexcept;

    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    size_t getSerialSize(unsigned version) const noexcept;

    /**
     * Serializes this instance to an encoder.
     * @param[in] encoder   Encoder
     * @param[in] version   Serialization version
     * @execptionsafety Basic guarantee
     * @threadsafety    Compatible but not thread-safe
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
    static ProdInfo deserialize(
            Decoder&        decoder,
            const unsigned  version);
};

} // namespace

#endif /* PRODINFO_H_ */
