/**
 * This file declares the name of a data-product.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: ProdName.h
 *  Created on: Nov 6, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROD_PRODNAME_H_
#define MAIN_PROD_PRODNAME_H_

#include "error.h"

#include <string>

namespace hycast {

class ProdName final : public Serializable<ProdName>
{
    std::string name;

    ProdName(
            Decoder&       decoder,
            const unsigned version)
    {
        decoder.decode(name);
        if (name.length() > prodNameMax)
            throw INVALID_ARGUMENT("Name too long: length=" +
                    std::to_string(name.length()) + ", max=" +
                    std::to_string(prodNameMax));
    }

public:
    static const uint16_t prodNameMax = UINT16_MAX;

    inline ProdName()
        : name{}
    {}

    inline ProdName(const std::string& name)
        : name{name}
    {
        if (name.length() > prodNameMax)
            throw INVALID_ARGUMENT("Name too long: length=" +
                    std::to_string(name.length()) + ", max=" +
                    std::to_string(prodNameMax));
    }

    inline ProdName(const char* name)
        : ProdName{std::string{name}}
    {}

    inline size_t length() const noexcept
    {
        return name.length();
    }

    inline operator std::string() const
    {
        return name;
    }

    inline const std::string to_string() const
    {
        return name;
    }

    inline const char* c_str() const noexcept
    {
        return name.c_str();
    }

    /**
     * Compares this instance with another.
     * @param[in] that  Other instance
     * @retval 0        They compare equal
     * @retval <0       Either the value of the first character that does not
     *                  match is lower in this instance, or all compared
     *                  characters match but this instance is shorter
     * @retval >0       Either the value of the first character that does not
     *                  match is greater in this instance, or all compared
     *                  characters match but this instance is longer
     */
    inline int compare(const ProdName& that) const
    {
        return name.compare(that.name);
    }

    /**
     * Returns the number of bytes in the serial representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the number of bytes in the serial representation
     */
    inline size_t getSerialSize(unsigned version) const noexcept {
        return Codec::getSerialSize(name);
    }

    inline size_t serialize(
            Encoder&       encoder,
            const unsigned version) const
    {
        return encoder.encode(name);
    }

    inline static ProdName deserialize(
            Decoder&       decoder,
            const unsigned version)
    {
        return ProdName{decoder, version};
    }
};

} // namespace

#endif /* MAIN_PROD_PRODNAME_H_ */
