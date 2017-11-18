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
#include "Product.h"
#include "ProdRcvr.h"

#include <memory>

namespace hycast {

class ProdStore final
{
#define DEFAULT_MIN_RESIDENCE 3600.0
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    class ChunkInfoIterator
    {
        friend ProdStore::Impl;

        class                 Impl;
        std::shared_ptr<Impl> pImpl;

        ChunkInfoIterator(Impl* impl);

    public:
        /**
         * @retval ChunkInfo{}  No such chunk exists
         * @return              Information on a chunk of data
         */
        const ChunkId operator *();
        ChunkInfoIterator& operator ++();
    };

    /**
     * Status of an addition to the product-store
     */
    class AddStatus
    {
        unsigned              status;
        static const unsigned IS_COMPLETE = 1;
        static const unsigned IS_NEW = 2;
    public:
        inline AddStatus() : status{0}   {}
        inline AddStatus& setNew()       { status |= IS_NEW; return *this; }
        inline AddStatus& setComplete()  { status |= IS_COMPLETE; return *this; }
        inline bool isNew()       const  { return status & IS_NEW; }
        inline bool isComplete()  const  { return status & IS_COMPLETE; }
    };

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
    explicit ProdStore(
            const std::string& pathname,
            const double       residence = DEFAULT_MIN_RESIDENCE);

    /**
     * Constructs. The product-store will not be written to a persistence-file
     * upon destruction in order to persist the store between sessions.
     * @param[in] residence   Desired minimum residence-time, in seconds, of
     *                        data-products
     * @throw InvalidArgument Residence-time is negative
     * @see ProdStore(const std::string& pathname, double residence)
     */
    explicit ProdStore(const double residence = DEFAULT_MIN_RESIDENCE)
        : ProdStore("", residence)
    {}

    /**
     * Adds an entire product. Does nothing if the product has already been
     * added. If added, the product will be removed when the minimum residence
     * time has elapsed.
     * @param[in] prod   Product to be added
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    void add(Product& prod);

    /**
     * Adds product-information to an entry. Creates the entry if it doesn't
     * exist.
     * @param[in] prodInfo  Product information
     * @param[out] prod     Associated product. Set iff the return status
     *                      indicates the product is complete.
     * @return              Status of the addition
     * @exceptionsafety     Basic guarantee
     * @threadsafety        Safe
     * @see                 `ProdStore::AddStatus`
     */
    AddStatus add(const ProdInfo& prodInfo, Product& prod);

    /**
     * Adds a latent chunk of data to a product. Creates the product if it
     * doesn't already exist. Will not overwrite an existing chunk of data in
     * the product.
     * @param[in]  chunk  Latent chunk of data to be added
     * @param[out] prod   Associated product. Set iff the return status
     *                    indicates the product is complete.
     * @return            Status of the addition
     * @exceptionsafety   Strong guarantee
     * @threadsafety      Safe
     * @see               `ProdStore::AddStatus`
     */
    AddStatus add(LatentChunk& chunk, Product& prod);

    /**
     * Returns the number of products in the store -- both complete and
     * incomplete.
     * @return Number of products in the store
     */
    size_t size() const noexcept;

    /**
     * Returns product-information on a given data-product.
     * @param[in]  index  Index of the data-product
     * return             Product information. Will be invalid if no such
     *                    data-product exists.
     * @see `ProdInfo::operator bool()`
     */
    ProdInfo getProdInfo(const ProdIndex index) const;

    /**
     * Indicates if this instance contains a given chunk of data.
     * @param[in] info  Information on the chunk
     * @retval `true`   Chunk exists
     * @retval `false`  Chunk doesn't exist
     */
    bool haveChunk(const ChunkId& info) const;

    /**
     * Returns the identified chunk of data.
     * @param[in]  id     Chunk identifier
     * @return            Data-chunk. Will be invalid if no such chunk exists.
     * @see `Chunk::operator bool()`
     */
    ActualChunk getChunk(const ChunkId& id) const;

    /**
     * Returns information on the oldest missing data-chunk.
     * @return  Information on the oldest missing data-chunk
     */
    ChunkId getOldestMissingChunk() const;

    ChunkInfoIterator getChunkInfoIterator(const ChunkId& startWith) const;
};

} // namespace

#endif /* MAIN_PROD_PRODSTORE_H_ */
