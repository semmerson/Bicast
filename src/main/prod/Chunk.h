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
     */
    ProdIndex_t getProdIndex() const noexcept;
    /**
     * Returns the index of the chunk-of-data.
     * @return the index of the chunk
     */
    ProdIndex_t getChunkIndex() const noexcept;
    /**
     * Returns the size of the chunk of data.
     * @return the size of the chunk of data
     * @exceptionsafety Strong
     * @threadsafety Safe
     */
    ChunkSize getSize() const noexcept;
    /**
     * Drains the chunk of data into a buffer. The latent data will no longer
     * be available.
     * @param[in] data  Buffer to drain the chunk of data into
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    void drainData(void* data);
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
     * @param[in] size  Amount of data in bytes
     */
    ActualChunk(
            const ChunkInfo& info,
            const void*      data,
            const ChunkSize  size);

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
    ProdIndex_t getProdIndex() const noexcept;

    /**
     * Returns the index of the chunk-of-data.
     * @return the index of the chunk
     */
    ProdIndex_t getChunkIndex() const noexcept;

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
     * Serializes this instance to an encoder.
     * @param[in] encoder  Encoder
     * @param[in] version  Protocol version
     * @exceptionsafety Basic
     * @threadsafety Compatible but not safe
     */
    void serialize(
            Encoder&       encoder,
            const unsigned version) const;
};

} // namespace

#endif /* CHUNK_H_ */
