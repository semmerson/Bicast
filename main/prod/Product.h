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

class Product final {
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs from nothing.
     */
    Product();

    /**
     * Constructs from information on a product.
     * @param[in] info Information on a product
     */
    explicit Product(const ProdInfo& info);

    /**
     * Constructs from complete data.
     * @param[in] name   Name of the product
     * @param[in] index  Product index
     * @param[in] data   Product data. Copied.
     * @param[in] size   Amount of data in bytes
     */
    Product(const std::string& name,
            const ProdIndex    index,
            const void*        data,
            const size_t       size);

    /**
     * Constructs from a file.
     * @param[in] pathname   Pathname of the file
     * @param[in] index      Product index
     */
    Product(const std::string& pathname,
            const ProdIndex    index);

    /**
     * Indicates if this instance is valid.
     */
    operator bool() const noexcept;

    /**
     * Sets the associated product-information providing it is consistent with
     * the information provided during construction (basically, only the name
     * can be changed).
     * @param[in] info      New product-information
     * @retval `false`      Duplicate information
     * @retval `true`       New information consistent with existing
     * @throw RuntimeError  `info` is inconsistent with existing information
     */
    bool set(const ProdInfo& info);

    /**
     * Returns information on the product.
     * @return Information on the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const ProdInfo& getInfo() const noexcept;

    /**
     * Returns the product's index.
     * @return          Product's index
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const ProdIndex getIndex() const noexcept;

    /**
     * Indicates if this instance is earlier than another.
     * @param[in] that   Other instance
     * @retval `true`    Yes
     * @retval `false`   No
     */
    bool isEarlierThan(const Product& that) const noexcept;

    /**
     * Identifies the earliest missing chunk of data.
     * @return           Information on the earliest missing chunk of data or
     *                   empty information if the product is complete.
     * @execptionsafety  Nothrow
     * @threadsafety     Compatible but not safe
     * @see `isComplete()`
     * @see `ChunkInfo::operator bool()`
     */
    ChunkInfo identifyEarliestMissingChunk() const noexcept;

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

    /**
     * Indicates if this instance contains a given chunk of data.
     * @param[in] index  Chunk index
     * @retval `true`    Chunk exists
     * @retval `false`   Chunk doesn't exist
     */
    bool haveChunk(const ChunkIndex index) const;

    /**
     * Returns the chunk of data corresponding to a chunk index.
     * @param[in] index   Chunk index
     * @param[out] chunk  Corresponding chunk of data
     * @retval `true`     Chunk exists. `chunk` is set.
     * @retval `false`    Chunk doesn't exist. `chunk` isn't set.
     */
    bool getChunk(
            const ChunkIndex index,
            ActualChunk&     chunk) const;
};

} // namespace

#endif /* MAIN_PROD_PRODUCT_H_ */
