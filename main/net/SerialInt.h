/**
 * This file declares a class template for serializable integers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Int.h
 *  Created on: Nov 3, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_SERIALINT_H_
#define MAIN_MISC_SERIALINT_H_

#include "Serializable.h"

#include <functional>
#include <stddef.h>
#include <string>

namespace hycast {

/**
 * @tparam T  Type of integer (e.g., `unsigned long`)
 */
template<class T>
class SerialInt : public Serializable<T>
{
    T value;

public:
    SerialInt(const T value = 0)
        : value{value}
    {}

    inline size_t serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        return encoder.encode(value);
    }

    inline operator T() const noexcept
    {
        return value;
    }

    inline size_t getSerialSize(unsigned version) const noexcept
    {
        return sizeof(T);
    }

    inline static T deserialize(
            Decoder&        decoder,
            const unsigned  version)
    {
        T value;
        decoder.decode(value);
        return value;
    }

    inline std::string to_string() const
    {
        return std::to_string(value);
    }

    inline size_t hash() const noexcept
    {
#if 1
        std::hash<T> valueHash;
        return valueHash(value);
#elif 0
        return std::hash<T>(value);
#else
        return std::hash(value);
#endif
    }
    inline bool operator ==(const SerialInt<T>& that) const noexcept {
        return value == that.value;
    }
    inline bool operator !=(const SerialInt<T>& that) const noexcept {
        return value != that.value;
    }
    inline SerialInt<T>& operator ++() noexcept {
        ++value;
        return *this;
    }
    inline SerialInt<T>& operator --() noexcept {
        --value;
        return *this;
    }

};

} // namespace

#endif /* MAIN_MISC_SERIALINT_H_ */
