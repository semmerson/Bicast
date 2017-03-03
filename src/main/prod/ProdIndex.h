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

class ProdIndex final : public Serializable<ProdIndex> {
    ProdIndex_t index;

public:
    /**
     * Constructs from a numeric product-index. The constuctor isn't explicit to
     * allow automatic conversions between ProdIndex and ProdIndex_t.
     */
    ProdIndex(ProdIndex_t index = 0);

    operator ProdIndex_t() const {
        return index;
    }

    /**
     * Returns the hash code of this instance.
     * @return This instance's hash code
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
    size_t getSerialSize(unsigned version) const noexcept {
        return sizeof(ProdIndex_t);
    }

    void serialize(
            Encoder&       encoder,
            const unsigned version) const;

    static ProdIndex deserialize(
            Decoder&       decoder,
            const unsigned version);
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
