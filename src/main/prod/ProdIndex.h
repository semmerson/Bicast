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
#include "RecStream.h"
#include "Serializable.h"

#include <cstdint>
#include <functional>

namespace hycast {

class ProdIndex final : public Serializable<ProdIndex>
{
public:
    typedef uint32_t  type;
    static const type prodIndexMax = UINT32_MAX;

    /**
     * Constructs. NB: Not explicit.
     * @param[in] index  Product index
     */
    ProdIndex(uint32_t index = 0) noexcept
        : index{index}
    {}

    /**
     * Copy constructs.
     * @param[in] that  Other instance
     */
    ProdIndex(const ProdIndex& that) noexcept
        : index{that.index}
    {}

    /**
     * Move constructs.
     * @param[in] that  Other instance
     */
    ProdIndex(const ProdIndex&& that) noexcept
        : index{that.index}
    {}

    /**
     * Copy assigns.
     * @param[in] that  Other instance
     */
    ProdIndex& operator =(const ProdIndex& rhs) noexcept
    {
        index = rhs.index;
        return *this;
    }

    /**
     * Move assigns.
     * @param[in] that  Other instance
     */
    ProdIndex& operator =(const ProdIndex&& rhs) noexcept
    {
        index = rhs.index;
        return *this;
    }

    /**
     * Converts.
     */
    operator uint32_t() const noexcept
    {
        return index;
    }

    std::string to_string() const
    {
        return std::to_string(index);
    }

    /**
     * Returns the hash code of this instance.
     * @return This instance's hash cod
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    size_t hash() const noexcept {
        return std::hash<decltype(index)>()(index);
    }

    bool operator ==(const ProdIndex& that) const noexcept {
        return index == that.index;
    }
    bool operator !=(const ProdIndex& that) const noexcept {
        return index != that.index;
    }
    bool operator <(const ProdIndex& that) const noexcept {
        return index < that.index;
    }
    bool operator <=(const ProdIndex& that) const noexcept {
        return index <= that.index;
    }
    bool operator >(const ProdIndex& that) const noexcept {
        return index > that.index;
    }
    bool operator >=(const ProdIndex& that) const noexcept {
        return index >= that.index;
    }
    ProdIndex& operator ++() noexcept {
        ++index;
        return *this;
    }
    ProdIndex& operator --() noexcept {
        --index;
        return *this;
    }

    /**
     * Returns the number of bytes in the serial representation of this
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
    size_t getSerialSize(unsigned version) const noexcept {
        return getStaticSerialSize(version);
    }

    size_t serialize(
            Encoder&       encoder,
            const unsigned version) const;

    static ProdIndex deserialize(
            Decoder&       decoder,
            const unsigned version);

private:
    type index;
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
