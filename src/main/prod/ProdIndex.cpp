/**
 * This file implements the product-index.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdIndex.cpp
 * @author: Steven R. Emmerson
 */

#include "ProdIndex.h"

#include <memory>
#include <netinet/in.h>
#include <stdexcept>

namespace hycast {

ProdIndex::ProdIndex(
        const char* const buf,
        const size_t      size,
        const unsigned    version)
{
    if (size < sizeof(ProdIndex_t))
        throw std::invalid_argument("Buffer too small for product-index: need="
                + std::to_string(sizeof(ProdIndex_t)) + " bytes, have=" +
                std::to_string(size));
    index = ntohl(*reinterpret_cast<const ProdIndex_t*>(buf));
}

char* ProdIndex::serialize(
        char*          buf,
        const size_t   size,
        const unsigned version) const
{
    if (size < sizeof(ProdIndex_t))
        throw std::invalid_argument("Buffer too small for product-index: need="
                + std::to_string(sizeof(ProdIndex_t)) + " bytes, have=" +
                std::to_string(size));
    *reinterpret_cast<ProdIndex_t*>(buf) = htonl(index);
    return buf + sizeof(ProdIndex_t);
}

ProdIndex ProdIndex::deserialize(
        const char* const buf,
        const size_t      size,
        const unsigned    version)
{
    return ProdIndex(buf, size, version);
}

} // namespace
