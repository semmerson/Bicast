/**
 * This file defines information about a product.
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
#include "Serializable.h"

#include <cstdint>
#include <istream>
#include <memory>
#include <ostream>
#include <string>

namespace hycast {

class ProdInfo : public Serializable {
    std::string name;
    ProdIndex   index;
    ProdSize    size;
    ChunkSize   chunkSize;
    static const int    IOVCNT = 4;

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
     * Constructs by deserializing a serialized representation from an input
     * stream.
     * @param[in] istream  Input stream
     * @param[in] version  Serialization version
     * @throws `istream` exceptions only
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    ProdInfo(
            std::istream&  istream,
            const unsigned version);
    /**
     * Constructs by deserializing a serialized representation from a buffer.
     * @param[in] buf      Buffer
     * @param[in] version  Serialization version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    ProdInfo(
            const void* const buf,
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
    size_t getSerialSize(unsigned version) const;
    /**
     * Serializes this instance to an output stream.
     * @param[in] ostream   Output stream
     * @param[in] version   Serialization version
     * @throws `ostream` exceptions only
     * @execptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    void serialize(
            std::ostream&  ostream,
            const unsigned version) const;
    /**
     * Serializes this instance to a buffer.
     * @param[in] buf       Buffer
     * @param[in] size      Buffer size in bytes
     * @param[in] version   Serialization version
     * @execptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    void serialize(
            void*          buf,
            const size_t   size,
            const unsigned version) const;
};

} // namespace

#endif /* PRODINFO_H_ */
