/**
 * A thread-safe file that contains a data-product.
 *
 *        File: ProdFile.h
 *  Created on: Dec 17, 2019
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

#ifndef MAIN_REPOSITORY_PRODFILE_H_
#define MAIN_REPOSITORY_PRODFILE_H_

#include "HycastProto.h"

#include <memory>

namespace hycast {

/**
 * Abstract product-file.
 */
class ProdFile
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    ProdFile(std::shared_ptr<Impl>&& impl) noexcept;

public:
    ProdFile() noexcept;

    ProdFile(const ProdFile& prodFile) noexcept;

    ProdFile(ProdFile&& prodFile) noexcept;

    virtual ~ProdFile();

    ProdFile& operator =(const ProdFile& rhs) noexcept;

    ProdFile& operator =(ProdFile&& rhs) noexcept;

    /**
     * Indicates if this instance is valid.
     *
     * @retval `false`     No
     * @retval `true`      Yes
     * @threadsafety       Safe
     * @cancellationpoint  No
     */
    operator bool() const noexcept;

    const std::string& getPathname() const noexcept;

    /**
     * Returns the size of the data-product in bytes.
     *
     * @return Size of the data-product in bytes
     */
    ProdSize getProdSize() const noexcept;

    /**
     * Returns the size, in bytes, of a data-segment.
     *
     * @param[in] offset         Offset to segment in bytes
     * @return                   Size of segment
     * @throws invalid_argument  Offset is invalid
     */
    SegSize getSegSize(const ProdSize offset) const;

    /**
     * Enables access to the underlying file.
     *
     * @param[in] rootFd  File descriptor open on the root directory
     */
    virtual void open(const int rootFd) const =0;

    /**
     * Disables access to the underlying file.
     */
    void close() const;

    /**
     * Indicates if the file contains a particular data-segment.
     *
     * @param[in] offset           Offset of the data-segment in bytes
     * @retval    `false`          No
     * @retval    `true`           Yes
     * @throws    IllegalArgument  Offset is invalid
     * @threadsafety               Safe
     * @exceptionsafety            Strong guarantee
     * @cancellationpoint          No
     */
    virtual bool exists(ProdSize offset) const =0;

    /**
     * Returns a pointer to a data-segment within the product.
     *
     * @param[in] offset            Segment's offset in bytes
     * @retval                      Pointer to data
     * @throws    InvalidArgument   Offset isn't multiple of segment-size,
     *                              offset isn't less than product-size, or
     *                              segment doesn't exist
     * @threadsafety                Safe
     * @exceptionsafety             Strong guarantee
     * @cancellationpoint           No
     */
    const char* getData(ProdSize offset) const;
};

/******************************************************************************/

/**
 * Product-file for the source of data-products.
 */
class SndProdFile final : public ProdFile
{
    class Impl;

public:
    SndProdFile() noexcept; // Will test false

    /**
     * Constructs from an existing file. Access to the underlying file is
     * closed.
     *
     * @param[in] rootFd        File descriptor open on root directory
     * @param[in] pathname      Pathname of file relative to root directory
     * @param[in] segSize       Size of a canonical segment in bytes
     * @throws    SystemError   Open failure
     * @cancellationpoint       No
     */
    SndProdFile(
            const int          rootFd,
            const std::string& pathname,
            SegSize            segSize);

    /**
     * Enables access to the underlying file.
     *
     * @param[in] rootFd  File descriptor open on the root directory
     */
    void open(const int rootFd) const override;

    bool exists(ProdSize offset) const override;
};

/******************************************************************************/

/**
 * Product-file for a receiver of data-products.
 */
class RcvProdFile final : public ProdFile
{
    class Impl;

public:
    RcvProdFile() noexcept; // Will test false

    /**
     * Constructs from product information. The instance is open.
     *
     * @param[in] rootFd           File descriptor open on root directory
     * @param[in] pathname         Pathname of file relative to the root directory
     * @param[in] prodSize         Product size in bytes
     * @param[in] segSize          Size of canonical data-segment in bytes
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @throws    SystemError      `open()` or `ftruncate()` failure
     */
    RcvProdFile(
            const int       rootFd,
            const String&   pathname,
            const ProdSize  prodSize,
            const SegSize   segSize);

    /**
     * Enables access to the underlying file.
     *
     * @param[in] rootFd  File descriptor open on the root directory
     */
    void open(const int rootFd) const override;

    /**
     * Indicates if the file contains a data-segment.
     *
     * @param[in] offset           Offset of data-segment in bytes
     * @retval    `true`           Yes
     * @retval    `false`          No
     * @throws    IllegalArgument  Offset is invalid
     * @threadsafety               Safe
     * @exceptionsafety            Strong guarantee
     * @cancellationpoint          No
     */
    bool exists(ProdSize offset) const override;

    /**
     * Indicates if the product has all the data.
     *
     * @retval `true`   The product is complete
     * @retval `false`  The product is not complete
     */
    bool isComplete() const;

    /**
     * Saves a data-segment.
     *
     * @param[in] dataSeg      Data-segment to be saved
     * @retval    `true`       This item is new and was saved
     * @retval    `false`      This item is old and was not saved
     * @throw InvalidArgument  Known product has a different size
     * @throw InvalidArgument  Segment's offset is invalid
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      Yes
     */
    bool save(const DataSeg& dataSeg) const;

    /**
     * Renames the associated file.
     *
     * @param[in] rootFd       File descriptor open on repository's root directory
     * @param[in] pathname     New pathname for the file
     * @throw SystemError      Couldn't rename product-file
     */
    void rename(
            const int     rootFd,
            const String& pathname);
};

} // namespace

#endif /* MAIN_REPOSITORY_PRODFILE_H_ */
