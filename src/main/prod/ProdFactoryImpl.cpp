/**
 * This file implements the implementation of a product factory, which creates
 * data-products from individual chunks-of-data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdFactoryImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "ProdFactoryImpl.h"

namespace hycast {

bool ProdFactoryImpl::add(const ProdInfo& prodInfo)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    ProdIndex_t prodIndex{prodInfo.getIndex()};
    decltype(prods)::iterator iter = prods.find(prodIndex);
    if (iter != prods.end())
        return false;
    prods.emplace(prodIndex, Product(prodInfo));
    return true;
}

ProdFactory::AddStatus ProdFactoryImpl::add(
        const ActualChunk& chunk,
        Product*           prod)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    ProdIndex_t prodIndex{chunk.getProdIndex()};
    decltype(prods)::iterator iter = prods.find(prodIndex);
    if (iter == prods.end())
        return ProdFactory::AddStatus::NO_SUCH_PRODUCT;
    iter->second.add(chunk);
    if (iter->second.isComplete()) {
        *prod = iter->second;
        prods.erase(prodIndex);
        return ProdFactory::AddStatus::PRODUCT_IS_COMPLETE;
    }
    return ProdFactory::AddStatus::PRODUCT_IS_INCOMPLETE;
}

bool ProdFactoryImpl::erase(const ProdIndex_t prodIndex)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    return prods.erase(prodIndex) == 1;
}

} // namespace
