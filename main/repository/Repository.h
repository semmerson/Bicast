/**
 * Repository of products.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Repository.h
 *  Created on: Sep 27, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_REPOSITORY_REPOSITORY_H_
#define MAIN_REPOSITORY_REPOSITORY_H_

#include "ProdFile.h"
#include "hycast.h"

#include <memory>
#include <string>

namespace hycast {

/**
 * Repository for temporary data-products.
 */
class Repository
{
protected:
    class Impl;

    std::shared_ptr<Impl> pImpl;

    Repository(Impl* impl);

public:
    virtual ~Repository() =default;

    /**
     * Returns the size of a canonical data-segment in bytes.
     *
     * @return Size of canonical data-segment in bytes
     */
    SegSize getSegSize() const noexcept;

    /**
     * Returns the pathname of the root-directory for this instance.
     *
     * @return             Pathname of root-directory
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    const std::string& getRootDir() const noexcept;

    /**
     * Returns the pathname of the file associated with a product name.
     *
     * @param[in] name     Product name
     * @return             Pathname of associated file
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  No
     */
    std::string getPathname(std::string name) const;

    /**
     * Returns the pathname of the file associated with a product index.
     *
     * @param[in] prodIndex  Product index
     * @return               Pathname of associated file
     * @threadsafety         Safe
     * @exceptionsafety      Strong guarantee
     * @cancellationpoint    No
     */
    std::string getPathname(ProdIndex prodIndex) const;

    /**
     * Returns information on a product.
     *
     * @param[in] prodIndex  Index of product
     * @return               Information on product. Will test false if no such
     *                       information exists.
     * @threadsafety         Safe
     * @exceptionsafety      Strong guarantee
     * @cancellationpoint    No
     * @see `ProdInfo::operator bool()`
     */
    virtual ProdInfo getProdInfo(ProdIndex prodIndex) const =0;

    /**
     * Returns a data-segment
     *
     * @param[in] segId             Segment identifier
     * @return                      Data-segment. Will test false if no such
     *                              segment exists.
     * @throws    InvalidArgument   Segment identifier is invalid
     * @threadsafety                Safe
     * @exceptionsafety             Strong guarantee
     * @cancellationpoint           No
     * @see `MemSeg::operator bool()`
     */
    virtual MemSeg getMemSeg(const SegId& segId) const =0;
};

/******************************************************************************/

/**
 * Publisher's repository.
 */
class PubRepo final : public Repository
{
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] root     Pathname of the root of the repository
     * @param[in] segSize  Size of canonical data-segment in bytes
     */
    PubRepo(const std::string& root,
            SegSize            segSize = 1460); // Max 4-byte UDP payload

    /**
     * Links to a file (which could be a directory) that's outside the
     * repository. The file or files will be eventually referenced by
     * `getNextProd()`.
     *
     * @param[in] filePath  Pathname of outside file
     * @param[in] prodName  Name of product
     * @see `getNextProd()`
     */
    void link(
            const std::string& filePath,
            const std::string& prodName);

    /**
     * Returns the index of the next product to publish. Blocks until one is
     * ready.
     *
     * @return Index of next product to publish
     */
    ProdIndex getNextProd() const;

    /**
     * Returns information on a product.
     *
     * @param[in] prodIndex  Index of product
     * @return               Information on product. Will test false if no such
     *                       information exists.
     * @threadsafety         Safe
     * @exceptionsafety      Strong guarantee
     * @cancellationpoint    No
     * @see `ProdInfo::operator bool()`
     */
    ProdInfo getProdInfo(ProdIndex prodIndex) const;

    /**
     * Returns a data-segment
     *
     * @param[in] segId             Segment identifier
     * @return                      Data-segment. Will test false if no such
     *                              segment exists.
     * @throws    InvalidArgument   Segment identifier is invalid
     * @threadsafety                Safe
     * @exceptionsafety             Strong guarantee
     * @cancellationpoint           No
     * @see `MemSeg::operator bool()`
     */
    MemSeg getMemSeg(const SegId& segId) const;
};

/******************************************************************************/

/**
 * Subscriber's repository.
 */
class SubRepo final : public Repository
{
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] rootPathname  Pathname of the root of the repository
     * @param[in] segSize       Size of canonical data-segment in bytes
     */
    SubRepo(const std::string& rootPathname,
            SegSize            segSize);

    /**
     * Saves product information.
     *
     * @param[in] prodInfo  Product information
     * @retval    `false`   Product information was not used
     * @retval    `true`    Product information was used
     * @threadsafety        Safe
     * @exceptionsafety     Strong guarantee
     * @cancellationpoint   No
     */
    bool save(const ProdInfo& prodInfo) const;

    /**
     * Saves a data-segment.
     *
     * @param[in] dataSeg  Data-segment
     * @retval    `false`  Data-segment was not used
     * @retval    `true`   Data-segment was used
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  No
     */
    bool save(DataSeg& dataSeg) const;

    /**
     * Returns information on a product.
     *
     * @param[in] prodIndex  Index of product
     * @return               Information on product. Will test false if no such
     *                       information exists.
     * @threadsafety         Safe
     * @exceptionsafety      Strong guarantee
     * @cancellationpoint    No
     * @see `ProdInfo::operator bool()`
     */
    ProdInfo getProdInfo(const ProdIndex prodIndex) const;

    /**
     * Returns a data-segment
     *
     * @param[in] segId             Segment identifier
     * @return                      Data-segment. Will test false if no such
     *                              segment exists.
     * @throws    InvalidArgument   Segment identifier is invalid
     * @threadsafety                Safe
     * @exceptionsafety             Strong guarantee
     * @cancellationpoint           No
     * @see `MemSeg::operator bool()`
     */
    MemSeg getMemSeg(const SegId& segId) const;

    /**
     * Indicates if product-information exists.
     *
     * @param[in] prodIndex  Index of the product in question
     * @retval    `false`    Product-information doesn't exist
     * @retval    `true`     Product-information does exist
     */
    bool exists(const ProdIndex prodIndex) const;

    /**
     * Indicates if a data-segment exists.
     *
     * @param[in] segId      Segment identifier
     * @retval    `false`    Data-segment doesn't exist
     * @retval    `true`     Data-segment does exist
     */
    bool exists(const SegId& segId) const;
};

} // namespace

#endif /* MAIN_REPOSITORY_REPOSITORY_H_ */
