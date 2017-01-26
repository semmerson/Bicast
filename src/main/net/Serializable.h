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

#include <cstddef>

namespace hycast {

template<class T>
class Serializable {
public:
    virtual ~Serializable() {}
    /**
     * Serializes this instance to a buffer
     * @param[out] buf      Output buffer. Shall be maximally aligned.
     * @param[in]  bufLen   Size of buffer in bytes
     * @param[in]  version  Protocol version
     * @return Address of next byte
     * @throws std::invalid_argument if the buffer is too small
     * @exceptionsafety Basic. `buf` might be modified.
     * @threadsafety    Safe
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
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    virtual size_t getSerialSize(unsigned version) const noexcept =0;

    /**
     * Returns a new instance corresponding to a serialized representation in a
     * buffer.
     * @param[in] buf      Buffer
     * @param[in] size     Size of buffer in bytes
     * @param[in] version  Protocol version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    static T deserialize(
            const char* const buf,
            const size_t      size,
            const unsigned    version);
};

} // namespace

#endif /* SERIALIZABLE_H_ */
