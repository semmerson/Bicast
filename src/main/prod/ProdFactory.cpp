/**
 * This file implements a product-factory, which creates data-products from
 * individual chunks of data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdFactory.cpp
 * @author: Steven R. Emmerson
 */

#include "ProdFactory.h"
#include "ProdFactoryImpl.h"

namespace hycast {

ProdFactory::ProdFactory()
    : pImpl{new ProdFactoryImpl()}
{}

bool hycast::ProdFactory::add(const ProdInfo& prodInfo)
{
    return pImpl->add(prodInfo);
}

ProdFactory::AddStatus ProdFactory::add(
        const ActualChunk& chunk,
        Product*           prod)
{
    return pImpl->add(chunk, prod);
}

bool ProdFactory::erase(const ProdIndex_t prodIndex)
{
    return pImpl->erase(prodIndex);
}

} // namespace
