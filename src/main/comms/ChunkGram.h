/**
 * This file declares a multicast datagram for a chunk-of-data -- one that
 * contains all necessary information to fully-allocate its associated data-
 * product in the absence of complete information on the product.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkGram.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_CHUNKGRAM_H_
#define MAIN_COMMS_CHUNKGRAM_H_

#include "ChunkInfo.h"
#include "HycastTypes.h"
#include "Serializable.h"

#include <memory>

namespace hycast {

class ChunkGram final : public Serializable<ChunkGram>
{
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     * @param[in] chunkInfo  Chunk information
     * @param[in] prodSize   Size, in bytes, of the associated data-product
     */
    ChunkGram(
            const ChunkInfo& chunkInfo,
            const ProdSize   prodSize);

    /**
     * Returns information on the associated chunk-of-data.
     * @return Information on the associated chunk-of-data
     */
    ChunkInfo getChunkInfo() const noexcept;

    /**
     * Returns the size of the associated data-product in bytes.
     * @return Size of the associated data-product in bytes
     */
    ProdSize getProdSize() const noexcept;

    /**
     * Returns the size, in bytes, of a serialized representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the size, in bytes, of a serialized representation of this
     *         instance
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t getSerialSize(unsigned version) const noexcept;

    /**
     * Serializes this instance to an encoder.
     * @param[out] encoder    Encoder
     * @param[in]  version    Protocol version
     * @return Number of bytes written
     * @exceptionsafety Basic Guarantee
     * @threadsafety    Safe
     */
    size_t serialize(
            Encoder&       encoder,
            const unsigned version) const;

    /**
     * Returns an instance corresponding to the serialized representation in a
     * decoder.
     * @param[in] decoder    Decoder
     * @param[in] version    Protocol version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    static ChunkGram deserialize(
            Decoder&        decoder,
            const unsigned  version);
};

} // namespace

#endif /* MAIN_COMMS_CHUNKGRAM_H_ */
