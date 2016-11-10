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

#include "HycastTypes.h"
#include "ProdIndex.h"
#include "Serializable.h"

#include <cstddef>
#include <memory>
#include <string>

namespace hycast {

class ProdInfoImpl; // Forward declaration

class ProdInfo : public Serializable {
    std::shared_ptr<ProdInfoImpl> pImpl;

public:
    /**
     * Constructs from nothing.
     */
    ProdInfo();
    /**
     * Constructs from information on a product.
     * @param[in] name       Product name
     * @param[in] index      Product index
     * @param[in] size       Size of product in bytes
     * @param[in] chunkSize  Size of data chunks in bytes
     * @throws std::invalid_argument if `name.size() > prodNameSizeMax`
     */
    ProdInfo(
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
    ProdInfo(
            const char* const buf,
            const size_t      size,
            const unsigned    version);
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
    ProdIndex getIndex() const;
    /**
     * Returns the size of the product in bytes.
     * @return Size of the product in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ProdSize getSize() const;
    /**
     * Returns the size of the product's data chunks in bytes.
     * @return Size of the product's data chunks in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    ChunkSize getChunkSize() const;
    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  The other instance
     * @retval true   This instance is equal to the other
     * @retval false  This instance is not equal to the other
     */
    bool equals(const ProdInfo& that) const;
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
     * @throws std::invalid_argument if the buffer is too small
     * @execptionsafety Basic. `buf` might have been modified.
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
    static ProdInfo deserialize(
            const char* const buf,
            const size_t      size,
            const unsigned    version);
};

} // namespace

#endif /* PRODINFO_H_ */
