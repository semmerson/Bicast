/**
 * An in-memory data-product.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: MemProd.h
 *  Created on: Sep 27, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_STORAGE_MEMPROD_H_
#define MAIN_STORAGE_MEMPROD_H_

#include "Chunk.h"

#include <cstddef>
#include <memory>

namespace hycast {

class MemProd
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    MemProd(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in] name         Name of the product
     * @param[in] size         Size of the product in bytes
     * @param[in] segSize      Size, in bytes, of every data-segment except,
     *                         usually, the last
     * @throw InvalidArgument  `segSize == 0 && prodSize != 0`
     */
    MemProd(const std::string& name,
            const size_t       size,
            const ChunkSize    segSize);

    /**
     * Returns the name of this product.
     *
     * @return Name of this product
     */
    const std::string& getName() const noexcept;

    /**
     * Accepts a data-chunk for incorporation.
     *
     * @param[in] chunk    Data-chunk
     * @retval    `true`   Chunk was incorporated
     * @retval    `false`  Chunk was previously incorporated. `log()` called.
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  No
     */
    bool accept(Chunk& chunk) const;

    /**
     * Indicates if this instance is complete (i.e., `accept()` has been called
     * for all segments).
     *
     * @retval `true`   Instance is complete
     * @retval `false`  Instance is not complete
     */
    bool isComplete() const noexcept;

    /**
     * Writes this data-product to a file descriptor.
     *
     * @param[in] fd       File descriptor
     * @threadsafety       Safe
     * @exceptionsafety    Basic guarantee
     * @cancellationpoint  Yes
     */
    void write(int fd) const;
};

} // namespace

#endif /* MAIN_STORAGE_MEMPROD_H_ */
