/**
 * A thread-safe file that contains a data-product.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: ProdFile.h
 *  Created on: Dec 17, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_REPOSITORY_PRODFILE_H_
#define MAIN_REPOSITORY_PRODFILE_H_

#include "hycast.h"

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

    ProdFile(Impl* impl);

public:
    virtual ~ProdFile() noexcept =0;

    /**
     * Indicates if this instance is valid.
     *
     * @retval `false`     No
     * @retval `true`      Yes
     * @threadsafety       Safe
     * @cancellationpoint  No
     */
    operator bool() noexcept;

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
     * Vets a data-segment.
     *
     * @param[in] offset    Offset of data-segment in bytes
     * @throws LOGIC_ERROR  Offset is greater than product's size or isn't an
     *                      integral multiple of canonical segment size
     */
    void vet(const ProdSize offset);

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
     * @throws    InvalidArgument   Offset isn't multiple of segment-size,
     *                              offset isn't less than product-size, or
     *                              segment doesn't exist
     * @threadsafety                Safe
     * @exceptionsafety             Strong guarantee
     * @cancellationpoint           No
     */
    const void* getData(ProdSize offset) const;
};

/******************************************************************************/

/**
 * Product-file for the source of data-products.
 */
class SndProdFile final : public ProdFile
{
    class Impl;

public:
    SndProdFile();

    /**
     * Constructs from an existing file.
     *
     * @param[in] pathname      Pathname of the file
     * @param[in] segSize       Size of a canonical segment in bytes
     * @throws    SystemError   Open failure
     * @cancellationpoint       No
     */
    SndProdFile(
            const std::string& pathname,
            SegSize            segSize);

    bool exists(ProdSize offset) const;
};

/******************************************************************************/

/**
 * Product-file for a receiver of data-products.
 */
class RcvProdFile final : public ProdFile
{
    class Impl;

public:
    RcvProdFile();

    /**
     * Constructs a new file.
     *
     * @param[in] pathname         Pathname of the file
     * @param[in] prodSize         Size of product in bytes
     * @param[in] segSize          Size of canonical data-segment
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @throws    SystemError      Open failure
     */
    RcvProdFile(
            const std::string& pathname,
            ProdSize           prodSize,
            SegSize            segSize);

    /**
     * Indicates if this instance is valid (i.e., not default-constructed).
     *
     * @retval `true`   Is valid
     * @retval `false`  Is not valid
     */
    operator bool() const noexcept;

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
    bool exists(ProdSize offset) const;

    /**
     * Saves a data-segment.
     *
     * @param[in] seg               Data-segment to be saved
     * @retval    `false`           Segment was not saved because it already
     *                              exists
     * @retval    `true`            Segment was saved
     * @throws    InvalidArgument   Segment's offset is invalid
     * @threadsafety                Safe
     * @exceptionsafety             Strong guarantee
     * @cancellationpoint           Yes
     */
    bool save(DataSeg& seg) const;

    /**
     * Sets the product information.
     *
     * @param[in] prodInfo  Product information
     */
    void setProdInfo(const ProdInfo& prodInfo) const;

    /**
     * Gets the product information.
     *
     * @return  Product information
     */
    const ProdInfo& getProdInfo() const;

    /**
     * Indicates if the product is complete (i.e., all data-segments exist and
     * the product-name is set).
     *
     * @retval `false`  No
     * @retval `true`   Yes
     */
    bool isComplete() const;

    /**
     * Closes the file-descriptor used to access the file. Idempotent.
     */
    void close() const;

    /**
     * Opens the file-descriptor used to access the file. Idempotent.
     */
    void open() const;
};

} // namespace

#endif /* MAIN_REPOSITORY_PRODFILE_H_ */
