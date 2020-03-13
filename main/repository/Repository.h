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
 * Repository of in-transit data-products.
 *
 * #tparam PF  Product-file type
 */
class Repository
{
protected:
    class Impl;

    std::shared_ptr<Impl> pImpl;

    Repository(Impl* impl);

public:
    virtual ~Repository()
    {}

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
     * Returns the pathname of the root-directory for data-product files whose
     * pathnames are based on data-product names.
     *
     * @return             Pathname of root-directory of named files
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    const std::string& getNamesDir() const noexcept;

    /**
     * Returns the pathname of the file associated with a product identifier.
     *
     * @param[in] prodId   Product identifier
     * @return             Pathname of associated file
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  No
     */
    std::string getPathname(ProdIndex prodId) const;

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

    /**
     * Indicates if information exists for a particular product.
     *
     * @param[in] prodIndex  Index of the product
     * @retval    `false`    Information does not exist
     * @retval    `true`     Information does exist
     */
    virtual bool exists(const ProdIndex prodIndex) const =0;

    /**
     * Indicates if a particular data-segment exists for a given product.
     *
     * @param[in] prodIndex  Index of the product
     * @retval    `false`    Information does not exist
     * @retval    `true`     Information does exist
     */
    virtual bool exists(const SegId& segId) const =0;
};

/******************************************************************************/

/**
 * Publisher-side repository.
 */
class PubRepo final : public Repository
{
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] rootPathname  Pathname of the root of the repository
     * @param[in] segSize       Size of canonical data-segment in bytes
     */
    PubRepo(const std::string& rootPathname,
            SegSize            segSize);

    /**
     * Processes the external creation of a new data-product.
     *
     * @param[in] prodName   Name of data-product. Must be under
     *                       `getNamesDir()`.
     * @param[in] prodIndex  Index of new data-product
     * @threadsafety                Safe
     * @exceptionsafety             Strong guarantee
     * @cancellationpoint           No
     * @see `getNamesDir()`
     */
    void newProd(
            const std::string& prodName,
            ProdIndex          prodIndex);

    bool exists(ProdIndex prodIndex) const;

    bool exists(const SegId& segId) const;

    /**
     * Returns information on a product.
     *
     * @param[in] prodIndex  Index of the product
     * @return               Corresponding information
     */
    ProdInfo getProdInfo(const ProdIndex prodIndex) const;

    /**
     * Returns a data segment.
     *
     * @param[in] segId  Segment identifier
     * @return           Corresponding segment
     */
    MemSeg getMemSeg(const SegId& segId) const;
};

/******************************************************************************/

/**
 * Interface for an observer of a subscriber-side repository.
 */
class SubRepoObs
{
public:
    virtual ~SubRepoObs()
    {}

    /**
     * Accepts notification that a data-product is complete.
     *
     * @param[in] prodInfo  Information on the completed data-product
     */
    virtual void completed(const ProdInfo& prodInfo) =0;
};

/**
 * Subscriber-side repository.
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
            SegSize            segSize,
            SubRepoObs&        repoObs);

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

    /**
     * Returns information on a product.
     *
     * @param[in] prodIndex  Index of the product
     * @return               Corresponding information
     */
    ProdInfo getProdInfo(const ProdIndex prodIndex) const;

    /**
     * Returns a data segment.
     *
     * @param[in] segId  Segment identifier
     * @return           Corresponding segment
     */
    MemSeg getMemSeg(const SegId& segId) const;
};

} // namespace

#endif /* MAIN_REPOSITORY_REPOSITORY_H_ */
