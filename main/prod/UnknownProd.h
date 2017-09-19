/**
 * This file defines an unknown product -- one that doesn't have any product
 * information. It's used to accumulate chunks-of-data for a product until the
 * product's information becomes available.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: UnknownProd.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_PROD_UNKNOWNPROD_H_
#define MAIN_PROD_UNKNOWNPROD_H_

#include "Chunk.h"
#include "HycastTypes.h"
#include "Product.h"

#include <map>
#include <memory>

namespace hycast {

class UnknownProd final
{
    class Chunk final
    {
        std::shared_ptr<char> data; // Chunk's actual data
        ChunkSize             size; // Size of actual data in bytes
    public:
        Chunk();
        bool      setIfNot(LatentChunk& latentChunk);
        void*     getData();
        ChunkSize getSize();
    };

    std::map<ChunkIndex, Chunk> chunks;

public:
    /**
     * Default constructs.
     */
    UnknownProd();

    /**
     * Adds a latent chunk of data.
     * @param[in] chunk  Latent chunk of data
     * @retval `true`   Chunk of data was added
     * @retval `false`  Chunk of data wasn't added
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    bool add(LatentChunk& chunk);

    /**
     * Makes a data-product based on product information and any previously-
     * added chunks of data. Upon return, this instance will not contain any
     * chunks of data.
     * @param[in] prodInfo  Product information
     * @return Product corresponding to the information and with any previously-
     *         added data-chunks
     * @exceptionsafety
     * @threadsafety
     */
    Product makeProduct(const ProdInfo& prodInfo);
};

} // namespace

#endif /* MAIN_PROD_UNKNOWNPROD_H_ */
