/**
 * This file declares a data-product.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
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

/**
 * Base class for data-products.
 */
class Product
{
    /*
     * NB: Not a pure virtual class so that instances of the base class (with
     * the "pImpl" idiom) can  exist.
     */
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    // The following constructor must not be defined here.
    Product(Impl* ptr);

public:
    /**
     * Default constructs.
     */
    Product()
        : pImpl{}
    {}

    /**
     * Copy constructs.
     * @param[in] prod  Other instance
     */
    Product(const Product& prod)
        : pImpl{prod.pImpl}
    {}

    virtual ~Product();

    /**
     * Indicates if this instance is valid (i.e., wasn't default-constructed).
     * @retval `true`   Instance is valid
     * @retval `false`  Instance is not valid
     */
    inline operator bool() const noexcept
    {
        return pImpl.operator bool();
    }

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
     * Indicates if this instance contains a given chunk of data.
     * @param[in] offset  Chunk offset
     * @retval `true`     Chunk exists
     * @retval `false`    Chunk doesn't exist
     */
    bool haveChunk(const ChunkOffset offset) const;

    /**
     * Indicates if this instance contains a given chunk of data.
     * @param[in] offset  Chunk index
     * @retval `true`     Chunk exists
     * @retval `false`    Chunk doesn't exist
     */
    bool haveChunk(const ChunkIndex index) const;

    /**
     * Returns the chunk of data corresponding to an index.
     * @param[in] index   Chunk index
     * @return            Data-chunk. Will be invalid if no such chunk exists.
     * @see `Chunk::operator bool()`
     */
    ActualChunk getChunk(const ChunkIndex index) const;

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
    ChunkId identifyEarliestMissingChunk() const;

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
     * Indicates if this instance is complete (i.e., contains
     * product-information and all chunks-of-data).
     * @return `true` iff this instance is complete
     */
    bool isComplete() const noexcept;

    /**
     * Returns a pointer to the data, which might contain garbage if
     * `isComplete()` is false.
     * @return  Pointer to data
     */
    const char* getData() const noexcept;
};

/******************************************************************************/

class MemoryProduct : public Product
{
    class Impl;

public:
    MemoryProduct();

    /**
     * Constructs from complete data.
     * @param[in] index      Data-product index
     * @param[in] name       Name of data-product
     * @param[in] size       Size of data-product
     * @param[in] data       Product data. Not copied. *Must* exist for the
     *                       lifetime of the constructed instance.
     * @param[in] chunkSize  Canonical size of a data-chunk
     */
    MemoryProduct(
            const ProdIndex index,
            const ProdName& name,
            const ProdSize  size,
            const char*     data,
            const ChunkSize chunkSize = ChunkSize::defaultSize);
};

/******************************************************************************/

class PartialProduct final : public Product
{
    class Impl;

public:
    PartialProduct();

    PartialProduct(const ProdInfo& prodInfo);
};

} // namespace

#endif /* MAIN_PROD_PRODUCT_H_ */
