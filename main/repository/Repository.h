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

#include "BicastProto.h"
#include "LastProd.h"
#include "ProdFile.h"

#include <memory>
#include <string>
#include <unistd.h>

namespace bicast {

/**
 * Class that contains both information on a product and access to its data.
 */
struct ProdEntry final
{
    ProdInfo  prodInfo;   ///< Product information
    ProdFile  prodFile;   ///< Product file

    ProdEntry()
        : prodFile{}
        , prodInfo()
    {}

    /**
     * Constructs.
     * @param[in] prodId    Product ID
     * @param[in] prodFile  Product file
     */
    explicit ProdEntry(
            const ProdId   prodId,
            ProdFile       prodFile = ProdFile())
        : prodInfo(prodId)
        , prodFile(prodFile)
    {}

    /**
     * Constructs.
     * @param[in] prodInfo  The product information
     * @param[in] prodFile  The product-file
     */
    ProdEntry(
            const ProdInfo& prodInfo,
            ProdFile&       prodFile)
        : prodInfo(prodInfo)
        , prodFile(prodFile)
    {}

    /**
     * Constructs.
     * @param[in] prodInfo  The product information
     * @param[in] prodFile  The product-file
     */
    ProdEntry(
            const ProdInfo&& prodInfo,
            ProdFile&&       prodFile)
        : prodFile(prodFile)
        , prodInfo(prodInfo)
    {}

    /**
     * Destroys.
     */
    ~ProdEntry() noexcept
    {}

    /// Indicates if this instance is valid.
    operator bool() const {
        return static_cast<bool>(prodInfo) && static_cast<bool>(prodFile);
    }

    /**
     * Copy assigns.
     * @param[in] rhs  The other, right-hand-side instance
     * @return         A reference to this just-assigned instance
     */
    inline ProdEntry& operator=(const ProdEntry& rhs) =default;

    /**
     * @return  Product metadata. Will test false if it hasn't been set by `set(const ProdInfo)`.
     * @see `set(const ProdInfo)`
     */
    inline ProdInfo getProdInfo() const {
        return prodInfo;
    }

    /**
     * Returns the pathname of the underlying product-file.
     * @return The pathname of the underlying product-file
     */
    inline const String& getPathname() const noexcept {
        return prodFile.getPathname();
    }

    /**
     * Returns a data segment.
     * @param[in] offset  Offset to the segment in bytes
     */
    inline DataSeg getDataSeg(const ProdSize offset) const {
        return DataSeg(DataSegId(prodInfo.getId(), offset), prodInfo.getSize(),
                prodFile.getData(offset));
    }

    /**
     * Returns all the data.
     * @return  All the data
     */
    inline const char* getData() const {
        return prodFile.getData();
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    inline std::string to_string() const {
        return prodInfo.to_string();
    }

    /// Returns the hash value of this instance.
    inline size_t hash() const noexcept {
        return prodInfo.getId().hash();
    }

    /// Indicates if this instance is considered the same as another.
    inline bool operator==(const ProdEntry& rhs) const noexcept {
        return prodInfo.getId() == rhs.prodInfo.getId();
    }
};

/**************************************************************************************************/

/**
 * Handle class for the base class of a repository for temporary data-products.
 */
class Repository
{
protected:
    class Impl;

    /// Smart pointer to the implementation
    std::shared_ptr<Impl> pImpl;

    Repository() noexcept =default;

    /**
     * Constructs.
     * @param[in] impl  Pointer to an implementation
     */
    Repository(Impl* impl) noexcept;

public:
    virtual ~Repository() =default;

    /**
     * Indicates if this instance is valid (i.e., not default constructed).
     *
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
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
    String getRootDir() const noexcept;

    /**
     * Returns the pathname of the top-level directory that contains only product-files.
     * @return The pathname of the top-level directory that contains only product-files
     */
    String getProdDir() const noexcept;

    /**
     * Returns the next product to process, either for transmission or local processing. Blocks
     * until one is available. The returned product is active.
     * @return               The next product to process. Will test false if halt() has been called;
     *                       otherwise, the product's metadata and data will be complete.
     * @throws SystemError   System failure
     * @throws RuntimeError  inotify(7) failure
     * @see halt()
     */
    ProdEntry getNextProd() const;

    /**
     * Causes getNextProd() to always return a ProdEntry that tests false.
     * @see getNextProd()
     */
    void halt() const;

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
    ProdIdSet subtract(const ProdIdSet other) const;

    /**
     * Returns the set of identifiers of complete products.
     *
     * @return             Set of complete product identifiers
     */
    ProdIdSet getProdIds() const;
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
     * Constructs. Upon return, the repository has been completely scanned and all relevant
     * directories are being watched.
     *
     * @param[in] rootDir       Pathname of the root-directory of the repository
     * @param[in] lastProcTime  Modification-time of the last, successfully-processed product-file
     */
    PubRepo(const String&      rootDir,
            const SysTimePoint lastProcTime = SysTimePoint::min());

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Links to a file (which could be a directory) that's outside the repository. The watcher will
     * notice and process the link.
     *
     * @param[in] pathname       Absolute pathname (with no trailing separator) of the file or
     *                           directory to be linked to
     * @param[in] prodName       Product name if the pathname references a file and Product name
     *                           prefix if the pathname references a directory
     * @throws InvalidArgument  `pathname` is empty or a relative pathname
     * @throws InvalidArgument  `prodName` is invalid
     */
    void link(
            const String& pathname,
            const String& prodName) const;
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
     * @param[in] rootPathname  Pathname of the root directory of the repository
     * @param[in] lastReceived  Saves information on the last received data-product
     * @param[in] queueProds    Queue complete data-products for return by getNextProd()?
     * @see getNextProd()
     */
    SubRepo(const std::string& rootPathname,
            const LastProdPtr& lastReceived,
            const bool         queueProds);

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Saves product-information in the corresponding product-file.
     *
     * @param[in] prodInfo     Product information
     * @retval    true         This item was saved
     * @retval    false        This item was not saved because it already exists
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
     * @retval    true         This item was saved
     * @retval    false        This item was not saved because it already exists
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
     * @retval    false      Product-information doesn't exist
     * @retval    true       Product-information does exist
     */
    bool exists(const ProdId prodId) const;

    /**
     * Indicates if a data-segment exists.
     *
     * @param[in] segId      Segment identifier
     * @retval    false      Data-segment doesn't exist
     * @retval    true       Data-segment does exist
     */
    bool exists(const DataSegId segId) const;

    /**
     * Returns the total number of products received.
     * @return  The total number of products received
     */
    long getTotalProds() const noexcept;

    /**
     * Returns the sum of the size of all products in bytes.
     * @return The sum of the size of all products in bytes
     */
    long long getTotalBytes() const noexcept;

    /**
     * Returns the sum of the latencies of all products in seconds.
     * @return The sum of the latencies of all products in seconds
     */
    double getTotalLatency() const noexcept;
};

} // namespace

#endif /* MAIN_REPOSITORY_REPOSITORY_H_ */
