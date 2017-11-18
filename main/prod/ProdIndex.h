/**
 * This file declares the index of a data-product.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdIndex.h
 * @author: Steven R. Emmerson
 */

#ifndef PRODINDEX_H_
#define PRODINDEX_H_

#include "SerialInt.h"
#include "Serializable.h"

#include <cstdint>
#include <functional>

namespace hycast {

class ProdIndex final : public Serializable<ProdIndex>
{
public:
    /**
     * At 1 product per nanosecond, a 64-bit unsigned integer is good for 584
     * years `(2**64-1)*1e-9/(86400*365.2524)` before it wraps around. This
     * should be enough to tide-over an offline instance.
     */
    typedef uint64_t  type;

private:
    SerialInt<type> index;

public:
    static const type prodIndexMax = UINT64_MAX;

    /**
     * Constructs. NB: Not explicit.
     * @param[in] index  Product index
     */
    inline ProdIndex(const type index = 0) noexcept
        : index{index}
    {}

    /**
     * Converts.
     */
    inline operator type() const noexcept
    {
        return static_cast<type>(index);
    }

    inline std::string to_string() const
    {
        return index.to_string();
    }

    /**
     * Returns the hash code of this instance.
     * @return          This instance's hash code
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    inline size_t hash() const noexcept {
        return index.hash();
    }

    inline bool operator ==(const ProdIndex& that) const noexcept {
        return index == that.index;
    }
    inline bool operator !=(const ProdIndex& that) const noexcept {
        return index != that.index;
    }
    /**
     * NB: Circular nature accounted for.
     */
    inline bool operator <(const ProdIndex& that) const noexcept {
        return that.index - index < prodIndexMax/2 && that.index != index;
    }
    /**
     * NB: Circular nature accounted for.
     */
    inline bool operator <=(const ProdIndex& that) const noexcept {
        return that.index - index < prodIndexMax/2;
    }
    /**
     * NB: Circular nature accounted for.
     */
    inline bool operator >(const ProdIndex& that) const noexcept {
        return index - that.index < prodIndexMax/2 && that.index != index;
    }
    /**
     * NB: Circular nature accounted for.
     */
    inline bool operator >=(const ProdIndex& that) const noexcept {
        return index - that.index < prodIndexMax/2;
    }
    inline ProdIndex& operator ++() noexcept {
        ++index;
        return *this;
    }
    inline ProdIndex& operator --() noexcept {
        --index;
        return *this;
    }

    inline bool isEarlierThan(const ProdIndex& that) const noexcept {
        return (that.index - index) <= prodIndexMax/2;
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
        return index.getSerialSize(version);
    }

    inline size_t serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        return index.serialize(encoder, version);
    }

    inline static ProdIndex deserialize(
            Decoder&       decoder,
            const unsigned version)
    {
        return SerialInt<type>::deserialize(decoder, version);
    }
};

} // namespace

namespace std {
    template<> struct hash<hycast::ProdIndex> {
        size_t operator()(const hycast::ProdIndex& index) const noexcept {
            return index.hash();
        }
    };

    template<> struct less<hycast::ProdIndex> {
        bool operator()(const hycast::ProdIndex& index1,
                const hycast::ProdIndex& index2) const noexcept {
            return index1 < index2;
        }
    };

    template<> struct equal_to<hycast::ProdIndex> {
        bool operator()(const hycast::ProdIndex& index1,
                const hycast::ProdIndex& index2) const noexcept {
            return index1 == index2;
        }
    };
}

#endif /* PRODINDEX_H_ */
