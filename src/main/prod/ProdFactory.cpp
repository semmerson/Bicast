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

#include <mutex>
#include <unordered_map>

namespace hycast {

class ProdFactory::Impl
{
    std::unordered_map<ProdIndex, Product> prods;
    std::mutex                             mutex;

public:
    /**
     * Constructs from nothing.
     */
    Impl() =default;

    /**
     * Adds information on a product.
     * @param[in] prodInfo  Information on the product
     * @retval `true`  iff the product is new
     */
    bool add(const ProdInfo& prodInfo)
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        ProdIndex prodIndex{prodInfo.getIndex()};
        decltype(prods)::iterator iter = prods.find(prodIndex);
        if (iter != prods.end())
            return false;
        prods.emplace(prodIndex, Product(prodInfo));
        return true;
    }

    /**
     * Adds a chunk-of-data. Returns the product if it's complete but doesn't
     * delete it.
     * @param[in]  chunk  Chunk to be added
     * @param[out] prod   Complete product. Set iff return status is
     *                    `PRODUCT_IS_COMPLETE`.
     * @retval `NO_SUCH_PRODUCT`        No product corresponds to `chunk`
     * @retval `PRODUCT_IS_INCOMPLETE`  Product is still incomplete
     * @retval `PRODUCT_IS_COMPLETE`    Product is complete. `prod` is set.
     */
    ProdFactory::AddStatus add(
            const ActualChunk& chunk,
            Product*           prod)
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        ProdIndex prodIndex{chunk.getProdIndex()};
        decltype(prods)::iterator iter = prods.find(prodIndex);
        if (iter == prods.end())
            return ProdFactory::AddStatus::NO_SUCH_PRODUCT;
        iter->second.add(chunk);
        if (iter->second.isComplete()) {
            *prod = iter->second;
            return ProdFactory::AddStatus::PRODUCT_IS_COMPLETE;
        }
        return ProdFactory::AddStatus::PRODUCT_IS_INCOMPLETE;
    }

    /**
     * Deletes a product.
     * @param[in] prodIndex  Index of product to be deleted
     * @retval `true` iff the product existed
     */
    bool erase(const ProdIndex prodIndex)
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        return prods.erase(prodIndex) == 1;
    }
};

ProdFactory::ProdFactory()
    : pImpl{new Impl()}
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

bool ProdFactory::erase(const ProdIndex prodIndex)
{
    return pImpl->erase(prodIndex);
}

} // namespace
