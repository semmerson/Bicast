/**
 * A data-product that resides in memory.
 *
 *        File: MemProd.cpp
 *  Created on: Oct 21, 2019
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

#include "config.h"

#include "error.h"
#include "MemProd.h"
#include "Socket.h"

#include <string>
#include <unistd.h>
#include <vector>

namespace hycast {

typedef uint64_t ProdSize;

class ProdInfo
{
    ProdSize    prodSize; ///< Size of product in bytes
    std::string name;     ///< Name of product
    bool        isSet;    ///< Information is set?

public:
    ProdInfo()
        : prodSize{0}
        , name{}
        , isSet{false}
    {}

    /**
     * Constructs.
     *
     * @param[in] bytes        Bytes containing serialized product information
     * @param[in] nbytes       Number of bytes in `bytes`
     * @throw InvalidArgument  `nbytes` is too small
     */
    ProdInfo(
            const char*   bytes,
            const SegSize nbytes)
        : ProdInfo()
    {
        typedef struct {
            ProdSize prodSize;
            char     name[0];
        } ProdInfoBuf;
        const ProdInfoBuf* prodInfoBuf =
                reinterpret_cast<const ProdInfoBuf*>(bytes);

        if (nbytes < sizeof(ProdInfoBuf))
            throw INVALID_ARGUMENT("Insufficient bytes");

        prodSize = InetSock::ntoh(prodInfoBuf->prodSize);
        name.assign(prodInfoBuf->name, nbytes - sizeof(ProdSize));
        isSet = true;
    }

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    explicit operator bool() const noexcept
    {
        return isSet;
    }

    /**
     * Returns the product's name.
     * @return The product's name
     */
    const std::string& getName() const
    {
        return name;
    }

    /**
     * Returns the size of the product in bytes.
     * @return The size of the product in bytes
     */
    ProdSize getProdSize() const
    {
        return prodSize;
    }
};

/// An implementation of a data product that resides in memory
class MemProd::Impl
{
    ProdInfo          prodInfo;
    SegSize           segSize;         ///< Byte-size of canonical data segment
    mutable SegIndex  numDataSegs;     ///< Number of data segments in product
                                       ///< (excludes prod-info segment)
    mutable SegSize   lastDataSegSize; ///< Size of last data segment in bytes
    char*             data;            ///< Product's data
    std::vector<bool> segLedger;       ///< What segments have been accepted
    SegIndex          numAccepted;     ///< Number of accepted segments

#if 0
    /**
     * Accepts a product-information segment for incorporation.
     *
     * @param[in] chunk        Chunk containing product-information segment to
     *                         be incorporated
     * @retval    true         Chunk was incorporated
     * @retval    false        Chunk was previously incorporated.
     *                         `chunk.write()` was not called.
     * @throw InvalidArgument  Chunk is too small to contain product information
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      No
     */
    bool acceptProdInfoSeg(Chunk& chunk)
    {
        if (prodInfo)
            return false;

        auto size = chunk.getSegSize();
        char buf[size];

        chunk.write(buf);

        prodInfo = ProdInfo{buf, size};

        return true;
    }

    /**
     * Accepts a data segment for incorporation.
     *
     * @param[in] chunk        Chunk containing data-segment to be incorporated
     * @retval    true         Chunk was incorporated
     * @retval    false        Chunk was previously incorporated.
     *                         `chunk.write()` was not called.
     * @throw InvalidArgument  Segment index is too large. `chunk.write()` was
     *                         not called.
     * @throw InvalidArgument  Segment size is unexpected. `chunk.write()` was
     *                         not called.
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      No
     */
    bool acceptDataSeg(Chunk& chunk)
    {
        // Data segment indexes in chunk are origin-1
        auto index = chunk.getSegIndex() - 1;

        if (index >= numDataSegs)
            throw INVALID_ARGUMENT("Too large data-segment index: index: " +
                    std::to_string(index) + ", numDataSegs: " +
                    std::to_string(numDataSegs));

        if (segLedger[index])
            return false;

        auto size = chunk.getSegSize();
        auto expected = (index == numDataSegs - 1) ? lastDataSegSize : segSize;

        if (size != expected)
            throw INVALID_ARGUMENT("Unexpected segment size: expected: " +
                    std::to_string(expected) + ", actual: " +
                    std::to_string(size));

        chunk.write(data + index*segSize);
        segLedger.at(index) = true;
        ++numAccepted;

        return true;
    }
#endif

public:
    /**
     * Constructs.
     *
     * @param[in] segSize        Size, in bytes, of every data-segment except,
     *                           usually, the first (product information)
     *                           segment and the last data segment
     * @throw InvalidArgument    `segSize == 0`
     * @throw std::system_error  Out of memory
     */
    Impl(const SegSize segSize)
        : prodInfo()
        , segSize{segSize}
        , numDataSegs{0}
        , lastDataSegSize{0}
        , data{nullptr}
        , segLedger(0)
        , numAccepted{0}
    {
        if (segSize == 0)
            throw INVALID_ARGUMENT("Zero segment size");
    }

    ~Impl()
    {
        delete[] data;
    }

#if 0
        else {
            numDataSegs = (prodSize + segSize - 1) / segSize;
            lastDataSegSize = prodSize - (numDataSegs-1)*segSize;
            segLedger = std::vector<bool>(numDataSegs, false);
        }
#endif

#if 0
    /**
     * Accepts a chunk for incorporation.
     *
     * @param[in] chunk     Chunk to be incorporated
     * @retval    true      Chunk was incorporated
     * @retval    false     Chunk was previously incorporated. `log()` called.
     * @threadsafety        Safe
     * @exceptionsafety     Strong guarantee
     * @cancellationpoint   No
     */
    bool accept(Chunk& chunk)
    {
        return (chunk.getSegIndex() == 0)
                ? acceptProdInfoSeg(chunk)
                : acceptDataSeg(chunk);
    }
#endif

    /**
     * Indicates if this instance is complete (i.e., `accept()` has been called
     * for all segments).
     *
     * @retval true     Instance is complete
     * @retval false    Instance is not complete
     */
    bool isComplete() const noexcept
    {
        return prodInfo && numAccepted == numDataSegs;
    }

    /**
     * Returns the name of this product.
     *
     * @return            Name of this product
     * @throw LogicError  Name has not been set (product information segment
     *                    hasn't been accepted)
     */
    const std::string& getName() const
    {
        if (!prodInfo)
            throw LOGIC_ERROR("Product information segment has not been seen");

        return prodInfo.getName();
    }

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
    void write(int fd) const
    {
        if (!prodInfo)
            throw LOGIC_ERROR("Product information segment has not been seen");

        auto size = prodInfo.getProdSize();

        if (::write(fd, data, size) == -1)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(size) +
                    " bytes of product \"" + prodInfo.getName() +
                    "\" to file-descriptor " + std::to_string(fd));
    }
};

MemProd::MemProd(Impl* const impl)
    : pImpl{impl}
{}

MemProd::MemProd(const SegSize segSize)
    : pImpl{new Impl(segSize)}
{}

const std::string& MemProd::getName() const
{
    return pImpl->getName();
}

#if 0
bool MemProd::accept(Chunk& chunk) const
{
    return pImpl->accept(chunk);
}
#endif

bool MemProd::isComplete() const noexcept
{
    return pImpl->isComplete();
}

void MemProd::write(int fd) const
{
    return pImpl->write(fd);
}

} // namespace
