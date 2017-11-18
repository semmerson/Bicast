/**
 * This file declares the size of a data-product.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdSize.h
 * @author: Steven R. Emmerson
 */

#ifndef PRODSIZE_H_
#define PRODSIZE_H_

#include "SerialInt.h"
#include "Serializable.h"

#include <cstdint>
#include <functional>

namespace hycast {

class ProdSize final : public Serializable<ProdSize>
{
public:
    typedef uint32_t  type;

private:
    SerialInt<type> size;

public:
    static const type prodSizeMax = UINT32_MAX;

    /**
     * Constructs. NB: Not explicit.
     * @param[in] size  Size of data-product in bytes
     */
    inline ProdSize(const type size = 0) noexcept
        : size{size}
    {}

    operator type() const noexcept
    {
        return size;
    }

    inline std::string to_string() const
    {
        return size.to_string();
    }

    /**
     * Returns the hash code of this instance.
     * @return          This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    inline size_t hash() const noexcept {
        return size.hash();
    }

    inline bool operator ==(const ProdSize& that) const noexcept {
        return size == that.size;
    }
    inline bool operator !=(const ProdSize& that) const noexcept {
        return size != that.size;
    }
    inline bool operator <(const ProdSize& that) const noexcept {
        return size < that.size;
    }
    inline bool operator <=(const ProdSize& that) const noexcept {
        return size <= that.size;
    }
    inline bool operator >(const ProdSize& that) const noexcept {
        return size > that.size;
    }
    inline bool operator >=(const ProdSize& that) const noexcept {
        return size >= that.size;
    }

    /**
     * Returns the number of bytes in the serial representation of an
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    static size_t getStaticSerialSize(const unsigned version) noexcept {
        return Codec::getSerialSize(sizeof(type));
    }

    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    inline size_t getSerialSize(unsigned version) const noexcept {
        return size.getSerialSize(version);
    }

    inline size_t serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        return size.serialize(encoder, version);
    }

    inline static ProdSize deserialize(
            Decoder&       decoder,
            const unsigned version)
    {
        return SerialInt<type>::deserialize(decoder, version);
    }
};

} // namespace

namespace std {
    template<> struct hash<hycast::ProdSize> {
        size_t operator()(const hycast::ProdSize& size) const noexcept {
            return size.hash();
        }
    };

    template<> struct less<hycast::ProdSize> {
        bool operator()(const hycast::ProdSize& size1,
                const hycast::ProdSize& size2) const noexcept {
            return size1 < size2;
        }
    };

    template<> struct equal_to<hycast::ProdSize> {
        bool operator()(const hycast::ProdSize& size1,
                const hycast::ProdSize& size2) const noexcept {
            return size1 == size2;
        }
    };

    inline string to_string(const hycast::ProdSize& size) {
        return size.to_string();
    }
}

#endif /* PRODSIZE_H_ */
