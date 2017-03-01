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
        const ProdIndex_t index)
    : index{index}
{}

void ProdIndex::serialize(
        Encoder&       encoder,
        const unsigned version)
{
    encoder.encode(index);
}

ProdIndex ProdIndex::deserialize(
        Decoder&       decoder,
        const unsigned version)
{
    ProdIndex_t netIndex;
    decoder.decode(netIndex);
    return ProdIndex(netIndex);
}

} // namespace
