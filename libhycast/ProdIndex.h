/**
 * This file declares the product-index.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdIndex.h
 * @author: Steven R. Emmerson
 */

#ifndef PRODINDEX_H_
#define PRODINDEX_H_

#include "HycastTypes.h"
#include "Serializable.h"

#include <cstdint>

namespace hycast {

class ProdIndex final : public Serializable {
    ProdIndex_t              index;
public:
    /**
     * Constructs from a numeric product-index. The constuctor isn't explicit to
     * allow automatic conversions between ProdIndex and ProdIndex_t.
     */
    ProdIndex(ProdIndex_t index = 0)
        : index(index) {}
    /**
     * Constructs by deserializing a serialized representation from a buffer.
     * @param[in] buf      Buffer
     * @param[in] version  Serialization version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    ProdIndex(
            const char* const buf,
            const size_t      size,
            const unsigned    version);
    operator ProdIndex_t() const {
        return index;
    }
    bool operator ==(const ProdIndex& that) const {
        return index == that.index;
    }
    bool operator !=(const ProdIndex& that) const {
        return index != that.index;
    }
    bool operator <(const ProdIndex& that) const {
        return index < that.index;
    }
    bool operator <=(const ProdIndex& that) const {
        return index <= that.index;
    }
    bool operator >(const ProdIndex& that) const {
        return index > that.index;
    }
    bool operator >=(const ProdIndex& that) const {
        return index >= that.index;
    }
    ProdIndex& operator ++() {
        ++index;
        return *this;
    }
    ProdIndex& operator --() {
        --index;
        return *this;
    }
    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    size_t getSerialSize(unsigned version) const {
        return sizeof(ProdIndex_t);
    }
    /**
     * Serializes this instance to a buffer.
     * @param[in] buf       Buffer
     * @param[in] size      Buffer size in bytes
     * @param[in] version   Serialization version
     * @return Number of bytes serialized
     * @execptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    char* serialize(
            char*          buf,
            const size_t   size,
            const unsigned version) const;
    /**
     * Initializes this instance based on a serialized representation in a
     * buffer.
     * @param[in] buf      Buffer
     * @param[in] size     Size of buffer in bytes
     * @param[in] version  Protocol version
     * @return Address of next byte
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    ProdIndex deserialize(
            const char* const buf,
            const size_t      size,
            const unsigned    version);
};

} // namespace

#endif /* PRODINDEX_H_ */
