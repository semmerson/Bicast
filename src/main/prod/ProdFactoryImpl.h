/**
 * This file declares the implementation of a product factory, which creates
 * data-products from individual chunks of data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdFactoryImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_PROD_PRODFACTORYIMPL_H_
#define MAIN_PROD_PRODFACTORYIMPL_H_

#include "ProdFactory.h"

#include <mutex>
#include <unordered_map>

namespace hycast {

class ProdFactoryImpl {
    std::unordered_map<ProdIndex_t, Product> prods;
    std::mutex                               mutex;
public:
    /**
     * Constructs from nothing.
     */
    ProdFactoryImpl() =default;
    /**
     * Adds information on a product.
     * @param[in] prodInfo  Information on the product
     * @retval `true`  iff the product is new
     */
    bool add(const ProdInfo& prodInfo);
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
            Product*           prod);
    /**
     * Deletes a product.
     * @param[in] prodIndex  Index of product to be deleted
     * @retval `true` iff the product existed
     */
    bool erase(const ProdIndex_t prodIndex);
};

} // namespace

#endif /* MAIN_PROD_PRODFACTORYIMPL_H_ */
