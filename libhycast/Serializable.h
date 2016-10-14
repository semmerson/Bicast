/**
 * This file declares an interface for classes that can be serialized.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Serializable.h
 * @author: Steven R. Emmerson
 */

#ifndef SERIALIZABLE_H_
#define SERIALIZABLE_H_

#include "Socket.h"

#include <cstddef>

namespace hycast {

class Serializable {
public:
    Serializable() {}
    virtual ~Serializable() {}
    /**
     * Serializes this instance to a buffer
     * @param[out] buf      Output buffer. Shall be maximally aligned.
     * @param[in]  bufLen   Size of buffer in bytes
     * @param[in]  version  Protocol version
     * @return Address of next byte
     */
    virtual char* serialize(
            char*          buf,
            const size_t   bufLen,
            const unsigned version) const =0;
    /**
     * Returns the size, in bytes, of a serialized representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the size, in bytes, of a serialized representation of this
     *         instance
     */
    virtual size_t getSerialSize(unsigned version) const =0;
};

} // namespace

#endif /* SERIALIZABLE_H_ */
