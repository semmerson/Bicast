/**
 * This file declares a store of data-products that can persist between
 * sessions.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdStore.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_PROD_PRODSTORE_H_
#define MAIN_PROD_PRODSTORE_H_

#include "Chunk.h"
#include "ProdInfo.h"
#include "ProdRcvr.h"

#include <memory>

namespace hycast {

class ProdStore final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs. If the given file isn't the empty string, then the
     * product-store will be written to it upon destruction in order to persist
     * the store between sessions.
     * @param[in] path        Pathname of file for persisting the product-store
     *                        between sessions or the empty string to indicate
     *                        no persistence
     * @param[in] residence   Desired minimum residence-time, in seconds, of
     *                        data-products
     * @throw SystemError     Couldn't open temporary persistence-file
     * @throw InvalidArgument Residence-time is negative
     */
    ProdStore(
            const std::string& pathname,
            const double       residence);

    /**
     * Constructs. If the given file isn't the empty string, then the
     * product-store will be written to it upon destruction in order to persist
     * the store between sessions.
     * @param[in] path        Pathname of file for persisting the product-store
     *                        between sessions or the empty string to indicate
     *                        no persistence.
     * @throw SystemError     Couldn't open temporary persistence-file
     * @see ProdStore(const std::string& pathname, double residence)
     */
    explicit ProdStore(const std::string& pathname)
        : ProdStore(pathname, 3600)
    {}

    /**
     * Constructs. The product-store will not be written to a persistence-file
     * upon destruction in order to persist the store between sessions.
     * @param[in] residence   Desired minimum residence-time, in seconds, of
     *                        data-products
     * @throw InvalidArgument Residence-time is negative
     * @see ProdStore(const std::string& pathname, double residence)
     */
    explicit ProdStore(const double residence)
        : ProdStore("", residence)
    {}

    /**
     * Destroys. Writes the product-store to the persistence-file if one was
     * specified during construction.
     */
    ~ProdStore() =default;

    /**
     * Adds product-information to an entry. Creates the entry if it doesn't
     * exist.
     * @param[in] prodInfo  Product information
     * @param[out] prod     Associated product
     * @retval true         Product is complete
     * @retval false        Product is not complete
     * @exceptionsafety     Basic guarantee
     * @threadsafety        Safe
     */
    bool add(const ProdInfo& prodInfo, Product& prod);

    /**
     * Adds a latent chunk of data to a product. Creates the product if it
     * doesn't already exist. Will not overwrite an existing chunk of data in
     * the product.
     * @param[in]  chunk  Latent chunk of data to be added
     * @param[out] prod   Associated product
     * @retval true       Product is complete
     * @retval false      Product is not complete
     * @exceptionsafety   Strong guarantee
     * @threadsafety      Safe
     */
    bool add(LatentChunk& chunk, Product& prod);

    /**
     * Returns the number of products in the store -- both complete and
     * incomplete.
     * @return Number of products in the store
     */
    size_t size() const noexcept;

    /**
     * Returns product-information on a given data-product.
     * @param[in]  index  Index of the data-product
     * @param[out] info   Information on the given product
     * @retval `true`     Information found. `info` is set.
     * @retval `false`    Information not found. `info` is not set.
     */
    bool getProdInfo(
            const ProdIndex index,
            ProdInfo&       info) const;

    /**
     * Indicates if this instance contains a given chunk of data.
     * @param[in] info  Information on the chunk
     * @retval `true`   Chunk exists
     * @retval `false`  Chunk doesn't exist
     */
    bool haveChunk(const ChunkInfo& info) const;

    /**
     * Returns the chunk of data corresponding to chunk-information.
     * @param[in]  info   Information on the desired chunk
     * @param[out] chunk  Corresponding chunk of data
     * @retval `true`     Chunk found. `chunk` is set.
     * @retval `false`    Chunk not found. `chunk` is not set.
     */
    bool getChunk(
            const ChunkInfo& info,
            ActualChunk&     chunk) const;
};

} // namespace

#endif /* MAIN_PROD_PRODSTORE_H_ */
