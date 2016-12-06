/**
 * This file declares a product-factory, which creates data-products from
 * individual chunks of data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdFactory.h
 * @author: Steven R. Emmerson
 */

#ifndef PROD_PRODFACTORY_H_
#define PROD_PRODFACTORY_H_

#include "Chunk.h"
#include "Product.h"

#include <memory>

namespace hycast {

class ProdFactoryImpl; // Forward declaration

class ProdFactory final {
    std::shared_ptr<ProdFactoryImpl> pImpl; // "pImpl" idiom
public:
    enum class AddStatus {
        NO_SUCH_PRODUCT,
        PRODUCT_IS_INCOMPLETE,
        PRODUCT_IS_COMPLETE
    };
    /**
     * Constructs from nothing.
     */
    ProdFactory();
    /**
     * Adds information on a product.
     * @param[in] prodInfo  Information on the product
     * @retval `true`  iff the product is new
     */
    bool add(const ProdInfo& prodInfo);
    /**
     * Adds a chunk-of-data. Returns the product if it's complete and deletes
     * it from this instance.
     * @param[in]  chunk  Chunk to be added
     * @param[out] prod   Complete product. Set iff return status is
     *                    `PRODUCT_IS_COMPLETE`.
     * @retval `NO_SUCH_PRODUCT`        No product corresponds to `chunk`
     * @retval `PRODUCT_IS_INCOMPLETE`  Product is still incomplete
     * @retval `PRODUCT_IS_COMPLETE`    Product is complete. `prod` is set and
     *                                  product is deleted from this instance.
     */
    AddStatus add(
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

#endif /* TEST_PROD_PRODFACTORY_H_ */
