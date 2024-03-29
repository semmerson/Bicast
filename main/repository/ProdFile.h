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

#include "BicastProto.h"

#include <memory>

namespace bicast {

/**
 * Abstract product-file.
 */
class ProdFile
{
public:
    class                 Impl;

private:
    std::shared_ptr<Impl> pImpl;

    ProdFile(Impl* pImpl) noexcept;

public:
     /// Default constructs.
    ProdFile() =default;

    /// Destroys
    virtual ~ProdFile() =default;

    /**
     * Constructs a product-file from an existing file. The instance is not open.
     *
     * @param[in] pathname    Pathname of product-file
     */
    ProdFile(const String& pathname);

    /**
     * Constructs a subscriber's product-file. Creates a new, underlying file -- and any necessary
     * antecedent directories --  from product-information. The file will have the given size and be
     * zero-filled. The instance is not open.
     *
     * @param[in] pathname         Pathname of the file
     * @param[in] prodSize         Product size in bytes
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @throws    SystemError      Couldn't open or truncate the file
     */
    ProdFile(
            const String&    pathname,
            const ProdSize   prodSize);

    /**
     * Indicates if this instance is valid.
     *
     * @retval false       No
     * @retval true        Yes
     * @threadsafety       Safe
     * @cancellationpoint  No
     */
    operator bool() const noexcept;

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    String to_string() const;

    /**
     * Returns the pathname of the underlying file.
     * @return The pathname of the underlying file
     */
    const std::string& getPathname() const noexcept;

    /**
     * Returns the size of the product in bytes.
     *
     * @return Size of the product in bytes
     */
    ProdSize getProdSize() const noexcept;

    /**
     * Closes the underlying file. Idempotent.
     */
    void close() const;

    /**
     * Returns the modification time of the underlying file.
     *
     * @return Modification time of the underlying file
     */
    SysTimePoint getModTime() const;

    /**
     * Sets the modification time of the underlying file.
     *
     * @param[in] modTime  Modification time for the underlying file
     */
    void setModTime(const SysTimePoint& modTime) const;

    /**
     * Indicates if the file contains a particular data-segment.
     *
     * @param[in] offset           Offset of the data-segment in bytes
     * @retval    false            No
     * @retval    true             Yes
     * @throws    IllegalArgument  Offset is invalid
     * @threadsafety               Safe
     * @exceptionsafety            Strong guarantee
     * @cancellationpoint          No
     */
    virtual bool exists(ProdSize offset) const;

    /**
     * Returns a pointer to a data-segment within the product. Opens the underlying file if
     * necessary.
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

    /**
     * Returns a pointer to all the data within the product. Opens the underlying file if
     * necessary.
     *
     * @retval                      Pointer to data
     * @threadsafety                Safe
     * @exceptionsafety             Strong guarantee
     * @cancellationpoint           No
     */
    const char* getData() const;

    /**
     * Saves a data-segment. Opens the underlying file if necessary.
     *
     * @param[in] dataSeg      Data-segment to be saved
     * @retval    true         This item is new and was saved
     * @retval    false        This item is old and was not saved
     * @throw InvalidArgument  Known product has a different size
     * @throw InvalidArgument  Segment's offset is invalid
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      Yes
     */
    bool save(const DataSeg& dataSeg) const;

    /**
     * Indicates if the product is complete.
     *
     * @retval true     The product is complete
     * @retval false    The product is not complete
     */
    bool isComplete() const;

    /**
     * Renames the underlying file.
     * @param[in] newPathname  The new pathname for the underlying file
     */
    void rename(const String& newPathname) const;

    /**
     * Deletes the underlying file.
     */
    void deleteFile() const;
};

} // namespace

#endif /* MAIN_REPOSITORY_PRODFILE_H_ */
