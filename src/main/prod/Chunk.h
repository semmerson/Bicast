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

#include "ChunkInfo.h"
#include "Codec.h"
#include "HycastTypes.h"
#include "SctpSock.h"

#include <memory>

namespace hycast {

/**
 * A chunk of data that must be read from an I/O object.
 */
class LatentChunk final
{
    class Impl; // Forward declaration of implementation

    std::shared_ptr<Impl> pImpl;

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
     * Returns information on the chunk.
     * @return information on the chunk
     * @exceptionsafety Strong
     * @threadsafety Safe
     */
    const ChunkInfo& getInfo() const noexcept;

    /**
     * Returns the index of the associated product.
     * @return the index of the associated product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ProdIndex getProdIndex() const noexcept;

    /**
     * Returns the size, in bytes, of the associated product.
     * @return the size, in bytes, of the associated product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ProdSize getProdSize() const noexcept;

    /**
     * Returns the byte-offset of the chunk-of-data.
     * @return the byte-offset of the chunk
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkOffset getOffset() const noexcept;

    /**
     * Returns the index of the chunk-of-data.
     * @return the index of the chunk
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkIndex getIndex() const noexcept;

    /**
     * Returns the size of the data-chunk in bytes.
     * @return Size of the data-chunk in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkSize getSize() const noexcept;

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
    void discard();

    /**
     * Indicates if this instance has data (i.e., whether or not `drainData()`
     * has been called).
     * @retval true   This instance has data
     * @retval false  This instance doesn't have data
     */
    bool hasData();
};

/**
 * An actual chunk of data.
 */
class ActualChunk final
{
    class Impl; // Forward declaration of implementation

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs from nothing.
     */
    ActualChunk();

    /**
     * Constructs from information on the chunk and a pointer to its data.
     * @param[in] info  Chunk information
     * @param[in] data  Chunk data
     */
    ActualChunk(
            const ChunkInfo& info,
            const void*      data);

    /**
     * Returns information on the chunk.
     * @return information on the chunk
     * @exceptionsafety Nothrow
     * @threadsafety Safe
     */
    const ChunkInfo& getInfo() const noexcept;

    /**
     * Returns the index of the associated product.
     * @return the index of the associated product
     */
    ProdIndex getProdIndex() const noexcept;

    /**
     * Returns the byte-offset of the chunk-of-data.
     * @return the byte-offset of the chunk
     */
    ChunkIndex getOffset() const noexcept;

    /**
     * Returns the size of the chunk of data.
     * @return the size of the chunk of data
     * @exceptionsafety Nothrow
     * @threadsafety Safe
     */
    ChunkSize getSize() const noexcept;

    /**
     * Returns a pointer to the data.
     * @returns a pointer to the data
     * @exceptionsafety Nothrow
     * @threadsafety Safe
     */
    const void* getData() const noexcept;

    /**
     * Returns the number of bytes in the serialized representation excluding
     * the actual bytes of data (i.e., the returned value is the number of
     * bytes in the serial representation of the chunk's information).
     * @param[in] version  Protocol version
     * @return Number of bytes in serial representation
     * @exceptionsafety No throw
     * @threadsafety    Safe
     */
    size_t getSerialSize(const unsigned version) const noexcept;

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
};

} // namespace

#endif /* CHUNK_H_ */
