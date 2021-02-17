/**
 * An in-memory data-product.
 *
 *        File: MemProd.h
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

#ifndef MAIN_STORAGE_MEMPROD_H_
#define MAIN_STORAGE_MEMPROD_H_

#include <hycast.h>
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
     * @param[in] segSize      Size, in bytes, of every data-segment except,
     *                         usually, the last
     * @throw InvalidArgument  `segSize == 0`
     */
    MemProd(const SegSize segSize);

    /**
     * Returns the name of this product.
     *
     * @return            Name of this product
     * @throw LogicError  Name has not been set (product information segment
     *                    hasn't been accepted)
     */
    const std::string& getName() const;

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
     * @throw LogicError   Name has not been set (product information segment
     *                     hasn't been accepted)
     * @throw SystemError  I/O failure
     * @threadsafety       Safe
     * @exceptionsafety    Basic guarantee
     * @cancellationpoint  Yes
     */
    void write(int fd) const;
};

} // namespace

#endif /* MAIN_STORAGE_MEMPROD_H_ */
