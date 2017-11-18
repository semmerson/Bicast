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

#include "Chunk.h"
#include "ProdIndex.h"
#include "ProdName.h"
#include "ProdSize.h"
#include "Serializable.h"

#include <cstddef>
#include <memory>
#include <string>

namespace hycast {

class ProdInfo final : public Serializable<ProdInfo>
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    ProdInfo();

    /**
     * Constructs.
     * @param[in] index        Product index
     * @param[in] name         Product name
     * @param[in] size         Size of product in bytes
     * @param[in] chunkSize    Size of canonical data-chunk in bytes
     * @throws InvalidArgument `name.size() > prodNameSizeMax`
     */
    ProdInfo(
            const ProdIndex index,
            const ProdName& name,
            const ProdSize  size,
            const ChunkSize chunkSize = ChunkSize::defaultChunkSize);

    /**
     * Constructs a partial instance. The name will be the empty string.
     * @param[in] index           Product index
     * @param[in] size            Size of product
     * @param[in] canonChunkSize  Size of a canonical data-chunk
     */
    ProdInfo(
            const ProdIndex index,
            const ProdSize  size,
            const ChunkSize canonChunkSize);

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval `true`   Instance is valid
     * @retval `false`  Instance is not valid
     */
    operator bool() const noexcept;

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
    const ProdName getName() const;

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
     * Returns the size of a canonical data-chunk.
     * @return          Size of canonical data-chunk
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkSize getChunkSize() const;

    /**
     * Returns the size, in bytes, of a given chunk-of-data.
     * @param[in] index        Index of data-chunk
     * @return                 Size of chunk in bytes
     * @throws InvalidArgument Index is invalid
     * @execeptionsafety       Strong guarantee
     * @threadsafety           Safe
     */
    ChunkSize getChunkSize(const ChunkIndex index) const;

    /**
     * Returns the number of chunks in the product.
     * @return the number of chunks in the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkIndex getNumChunks() const noexcept;

    /**
     * Returns the index of the chunk with a given offset.
     * @param[in] offset       Chunk-offset
     * @return                 Chunk-index
     * @throw InvalidArgument  Offset is invalid
     */
    ChunkIndex getChunkIndex(const ChunkOffset offset) const;

    /**
     * Returns the origin-0 index of a data-chunk.
     * @param[in] chunkId      Chunk identifier
     * @return                 Origin-0 index of data-chunk
     * @throw InvalidArgument  Invalid chunk identifier
     */
    ChunkIndex getChunkIndex(const ChunkId chunkId) const;

    /**
     * Returns the offset to a given data-chunk.
     * @param[in] index        Chunk index
     * @return                 Offset to data-chunk
     * @throw InvalidArgument  Chunk index is invalid
     */
    ChunkOffset getChunkOffset(const ChunkIndex index) const;

    /**
     * Returns the offset to a given data-chunk.
     * @param[in] chunkId  Chunk identifier
     * @return             Byte-offset to data-chunk
     */
    ChunkOffset getChunkOffset(const ChunkId chunkId) const;

    /**
     * Vets a chunk index.
     * @param[in] chunkIndex   Chunk index
     * @throw InvalidArgument  Chunk index is invalid
     */
    void vet(const ChunkIndex chunkIndex) const;

    /**
     * Returns information on a chunk.
     * @param chunkIndex  Chunk index
     * @return            Information on the chunk
     */
    ChunkInfo getChunkInfo(const ChunkIndex chunkIndex) const;

    /**
     * Returns chunk identifier.
     * @param[in] offset        Chunk offset
     * @return                  Chunk identifier
     * @throws InvalidArgument  Chunk offset is invalid
     * @execeptionsafety        Strong guarantee
     * @threadsafety            Safe
     */
    ChunkId makeChunkId(const ChunkOffset chunkOffset) const;

    /**
     * Returns chunk identifier.
     * @param[in] index         Chunk index
     * @return                  Chunk identifier
     * @throws InvalidArgument  Chunk index is invalid
     * @execeptionsafety        Strong guarantee
     * @threadsafety            Safe
     */
    ChunkId makeChunkId(const ChunkIndex index) const;

    /**
     * Indicates if this instance is considered equal to another except for,
     * perhaps, the name.
     * @param[in] that  Other instance
     * @retval true     Instances are equal
     * @retval false    Instances are not equal
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool equalsExceptName(const ProdInfo& that) const noexcept;

    /**
     * Indicates if this instance is considered equal to another.
     * @param[in] that  Other instance
     * @retval true     Instance is equal to other
     * @retval false    Instance is not equal to other
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
