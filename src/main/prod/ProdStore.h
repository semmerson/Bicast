/**
 * This file declares a store of data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdQueue.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_PROD_PRODSTORE_H_
#define MAIN_PROD_PRODSTORE_H_

#include "Chunk.h"
#include "ProdInfo.h"

#include <memory>

namespace hycast {

class ProdStore final
{
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    ProdStore();

    /**
     * Constructs from the pathname of the file for persisting the
     * product-queue.
     * @param[in] path  Pathname of persisting file
     */
    explicit ProdStore(const std::string pathname);

    /**
     * Destroys. Writes the product-queue to the persistence-file if one was
     * specified during construction.
     */
    ~ProdStore() =default;

    /**
     * Makes an initial entry for a product.
     * @param[in] prodInfo  Product information
     * @retval true   Success
     * @retval false  Entry already exists for product
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    bool add(const ProdInfo& prodInfo);

    /**
     * Adds a latent chunk of data to a product.
     * @param[in] chunk  Latent chunk of data to be added
     * @retval true      Success
     * @retval false     Chunk of data already exists for product
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    bool add(LatentChunk& chunk);
};

} // namespace

#endif /* MAIN_PROD_PRODSTORE_H_ */
