/**
 * Repository of products.
 *
 *        File: Repository.h
 *  Created on: Sep 27, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MAIN_REPOSITORY_REPOSITORY_H_
#define MAIN_REPOSITORY_REPOSITORY_H_

#include "HycastProto.h"
#include "ProdFile.h"

#include <memory>
#include <string>
#include <unistd.h>

namespace hycast {

/**
 * Handle class for the base class of a repository for temporary data-products.
 */
class Repository
{
protected:
    class Impl;

    std::shared_ptr<Impl> pImpl;

    Repository() noexcept;

    Repository(Impl* impl) noexcept;

public:
    /// Runtime parameters
    struct RunPar {
        String    rootDir;        ///< Pathname of root directory of repository
        int32_t   maxSegSize;     ///< Maximum size of a data-segment in bytes
        long      maxOpenFiles;   ///< Maximum number of open repository files
        int32_t   keepTime;       ///< Length of time to keep data-products in seconds
        RunPar( const String& rootDir,
                const int32_t maxSegSize,
                const long    maxOpenFiles,
                const int32_t keepTime)
            : rootDir(rootDir)
            , maxSegSize(maxSegSize)
            , maxOpenFiles(maxOpenFiles)
            , keepTime(keepTime)
        {}
    };

    virtual ~Repository() =default;

    /**
     * Indicates if this instance is valid (i.e., not default constructed).
     *
     * @retval `true`   This instance is valid
     * @retval `false`  This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Returns the maximum size of a data-segment in bytes.
     *
     * @return Size of canonical data-segment in bytes
     */
    SegSize getMaxSegSize() const noexcept;

    /**
     * Returns the absolute pathname of the root-directory for this instance.
     *
     * @return             Absolute pathname of root-directory
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    const std::string& getRootDir() const noexcept;

    /**
     * Returns the next product to process (i.e., either transmitted or locally processed).
     *
     * @return  Metadata of the next product to process
     */
    ProdInfo getNextProd() const;

    /**
     * Returns information on a product.
     *
     * @param[in] prodId     Product identifier
     * @return               Information on product. Will test false if no such
     *                       information exists.
     * @threadsafety         Safe
     * @exceptionsafety      Strong guarantee
     * @cancellationpoint    No
     * @see `ProdInfo::operator bool()`
     */
    ProdInfo getProdInfo(const ProdId prodId) const;

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
     * @see `DataSeg::operator bool()`
     */
    DataSeg getDataSeg(const DataSegId segId) const;

    /**
     * Returns a set of this instance's identifiers of complete products minus those of another set.
     *
     * @param[in]  other    Other set of product identifiers to be subtracted from this instance
     * @return              This instance's identifiers minus those of the other set
     */
    ProdIdSet::Pimpl subtract(const ProdIdSet::Pimpl other) const;

    /**
     * Returns the set of identifiers of complete products.
     *
     * @return             Set of complete product identifiers
     */
    ProdIdSet::Pimpl getProdIds() const;
};

/******************************************************************************/

/**
 * Handle class for a publisher's repository.
 */
class PubRepo final : public Repository
{
    class Impl;

public:
    /**
     * Default constructs. The returned instance will test false.
     */
    PubRepo();

    /**
     * Constructs.
     *
     * @param[in] rootDir       Pathname of the root-directory of the repository
     * @param[in] maxOpenFiles  Maximum number of files to have open simultaneously
     * @param[in] keepTime      Duration to keep products before deleting them in seconds
     */
    PubRepo(const String& rootDir,
            const long    maxOpenFiles,
            const int     keepTime = 3600);

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval `true`   This instance is valid
     * @retval `false`  This instance is not valid
     */
    operator bool() const noexcept;
};

/******************************************************************************/

/**
 * Handle class for a subscriber's repository.
 */
class SubRepo final : public Repository
{
    class Impl;

public:
    /**
     * Default constructs. The returned instance will test false.
     */
    SubRepo();

    /**
     * Constructs.
     *
     * @param[in] rootPathname  Pathname of the root of the repository
     * @param[in] maxOpenFiles  Maximum number of files to have open simultaneously
     * @param[in] keepTime      Duration, in seconds, to keep products before deleting them
     */
    SubRepo(const std::string& rootPathname,
            const size_t       maxOpenFiles,
            const int          keepTime = 3600);

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval `true`   This instance is valid
     * @retval `false`  This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Saves product-information in the corresponding product-file.
     *
     * @param[in] prodInfo     Product information
     * @retval    `true`       This item was saved
     * @retval    `false`      This item was not saved because it already exists
     * @throw InvalidArgument  Known product has a different size
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      No
     */
    bool save(const ProdInfo prodInfo) const;

    /**
     * Saves a data-segment in the corresponding product-file.
     *
     * @param[in] dataSeg      Data-segment
     * @retval    `true`       This item was saved
     * @retval    `false`      This item was not saved because it already exists
     * @throw InvalidArgument  Known product has a different size
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      No
     */
    bool save(DataSeg dataSeg) const;

    /**
     * Indicates if product-information exists.
     *
     * @param[in] prodId     Product identifier
     * @retval    `false`    Product-information doesn't exist
     * @retval    `true`     Product-information does exist
     */
    bool exists(const ProdId prodId) const;

    /**
     * Indicates if a data-segment exists.
     *
     * @param[in] segId      Segment identifier
     * @retval    `false`    Data-segment doesn't exist
     * @retval    `true`     Data-segment does exist
     */
    bool exists(const DataSegId segId) const;
};

} // namespace

#endif /* MAIN_REPOSITORY_REPOSITORY_H_ */
