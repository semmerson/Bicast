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

#include "Codec.h"

#include <cstddef>

namespace hycast {

template<class T>
class Serializable {
public:
    virtual ~Serializable() {}

    /**
     * Serializes this instance to an encoder.
     * @param[out] encoder    Encoder
     * @param[in]  version    Protocol version
     * @exceptionsafety Basic Guarantee
     * @threadsafety    Safe
     */
    virtual void serialize(
            Encoder&       encoder,
            const unsigned version) const =0;

    /**
     * Returns the size, in bytes, of a serialized representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the size, in bytes, of a serialized representation of this
     *         instance
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    virtual size_t getSerialSize(unsigned version) const noexcept =0;

    /**
     * Returns an instance corresponding to the serialized representation in a
     * decoder.
     * @param[in] decoder    Decoder
     * @param[in] version    Protocol version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    static T deserialize(
            Decoder&        decoder,
            const unsigned  version);
};

} // namespace

#endif /* SERIALIZABLE_H_ */
