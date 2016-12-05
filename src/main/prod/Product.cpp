/**
 * This file implements a data-product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Product.cpp
 * @author: Steven R. Emmerson
 */

#include "Product.h"
#include "ProductImpl.h"

namespace hycast {

Product::Product(const ProdInfo& info)
    : pImpl{new ProductImpl(info)}
{}

const ProdInfo& hycast::Product::getInfo() const noexcept
{
    return pImpl->getInfo();
}

bool Product::add(const ActualChunk& chunk)
{
    return pImpl->add(chunk);
}

bool Product::add(LatentChunk& chunk)
{
    return pImpl->add(chunk);
}

bool Product::isComplete() const
{
    return pImpl->isComplete();
}

} // namespace
