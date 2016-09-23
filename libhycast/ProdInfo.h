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

#include "Serializable.h"

#include <cstdint>
#include <istream>
#include <memory>
#include <string>

namespace hycast {

class ProdInfo : public Serializable {
    std::string name;
    uint32_t    index;
    uint32_t    size;
    uint16_t    chunkSize;
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
     */
    ProdInfo(
            const std::string& name,
            const uint32_t     index,
            const uint32_t     size,
            const uint16_t     chunkSize);
    /**
     * Constructs by deserializing a serialized representation from an SCTP
     * socket.
     * @param[in] sock     SCTP socket
     * @param[in] version  Serialization version
     * @throws std::underflow_error Serialized form is too small
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    ProdInfo(
            Socket&        sock,
            const unsigned version);
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
    uint32_t getIndex() const {return index;}
    /**
     * Returns the size of the product in bytes.
     * @return Size of the product in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    uint32_t getSize() const {return size;}
    /**
     * Returns the size of the product's data chunks in bytes.
     * @return Size of the product's data chunks in bytes
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    uint16_t getChunkSize() const {return chunkSize;}
    /**
     * Serializes this instance to an SCTP socket.
     * @param[in] sock      SCTP socket
     * @param[in] streamId  SCTP stream number to use
     * @param[in] version   Serialization version
     */
    void serialize(
            Socket&        sock,
            const unsigned streamId,
            const unsigned version) const;
};

} // namespace

#endif /* PRODINFO_H_ */
