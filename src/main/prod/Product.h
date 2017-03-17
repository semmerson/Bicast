/**
 * This file declares a data-product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Product.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_PROD_PRODUCT_H_
#define MAIN_PROD_PRODUCT_H_

#include "ProdInfo.h"
#include "Chunk.h"

#include <memory>

namespace hycast {

class ProductImpl; // Forward declaration

class Product final {
    std::shared_ptr<ProductImpl> pImpl; // "pImpl" idiom

public:
    /**
     * Constructs from nothing.
     */
    Product()
        : pImpl{}
    {}

    /**
     * Constructs from information on a product.
     * @param[in] info Information on a product
     */
    explicit Product(const ProdInfo& info);

    /**
     * Returns information on the product.
     * @return Information on the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const ProdInfo& getInfo() const noexcept;

    /**
     * Adds a chunk-of-data.
     * @param[in] chunk  The chunk
     * @return `true`    if the chunk of data was added
     * @return `false`   if the chunk of data had already been added. The
     *                   instance is unchanged.
     * @throws std::invalid_argument if the chunk is inconsistent with this
     *                               instance
     * @execptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    bool add(const ActualChunk& chunk);

    /**
     * Adds a latent chunk-of-data.
     * @param[in] chunk  The latent chunk
     * @return `true`    if the chunk of data was added
     * @return `false`   if the chunk of data had already been added. The
     *                   instance is unchanged.
     * @throws std::invalid_argument if the chunk is inconsistent with this
     *                               instance
     * @execptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    bool add(LatentChunk& chunk);

    /**
     * Indicates if this instance is complete (i.e., contains all
     * chunks-of-data).
     * @return `true` iff this instance is complete
     */
    bool isComplete() const;

    /**
     * Returns a pointer to the data.
     * @return a pointer to the data
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const char* getData() const noexcept;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  Other instance
     * @retval `true`   Yes
     * @retval `false`  No
     */
    bool operator==(const Product& that) const;
};

} // namespace

#endif /* MAIN_PROD_PRODUCT_H_ */
