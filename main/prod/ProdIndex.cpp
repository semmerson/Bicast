/**
 * This file implements the product-index.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
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

size_t ProdIndex::serialize(
        Encoder&       encoder,
        const unsigned version) const
{
    return encoder.encode(index);
}

ProdIndex ProdIndex::deserialize(
        Decoder&       decoder,
        const unsigned version)
{
    ProdIndex::type netIndex;
    decoder.decode(netIndex);
    return ProdIndex(netIndex);
}

} // namespace
