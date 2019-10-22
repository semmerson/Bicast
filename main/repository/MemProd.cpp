/**
 * 
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: MemProd.cpp
 *  Created on: Oct 21, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "MemProd.h"

#include <unistd.h>
#include <vector>

namespace hycast {

class MemProd::Impl
{
    const std::string name;        ///< Name of the product
    const size_t      prodSize;    ///< Size of the product in bytes
    const ChunkSize   segSize;     ///< Size of a canonical segment in bytes
    mutable SegIndex  numSegs;     ///< Number of segments in the product
    mutable ChunkSize lastSegSize; ///< Size of the last segment in bytes
    char*             data;        ///< Product's data
    std::vector<bool> segLedger;   ///< What segments have been accepted
    SegIndex          numAccepted; ///< Number of accepted segments

public:
    /**
     * Constructs.
     *
     * @param[in] name           Name of the product
     * @param[in] prodSize       Size of the product in bytes
     * @param[in] segSize        Size, in bytes, of every data-segment except,
     *                           maybe, the last
     * @throw InvalidArgument    `segSize == 0 && prodSize != 0`
     * @throw std::system_error  Out of memory
     */
    Impl(   const std::string& name,
            const size_t       prodSize,
            const ChunkSize    segSize)
        : name{name}
        , prodSize{prodSize}
        , segSize{segSize}
        , numSegs{0}
        , lastSegSize{0}
        , data{new char[prodSize]}
        , segLedger(0)
        , numAccepted{0}
    {
        if (segSize == 0) {
            if (prodSize != 0)
                throw INVALID_ARGUMENT("Zero segment size");
        }
        else {
            numSegs = (prodSize + segSize - 1) / segSize;
            lastSegSize = prodSize - (numSegs-1)*segSize;
            segLedger = std::vector<bool>(numSegs, false);
        }
    }

    ~Impl()
    {
        delete[] data;
    }

    /**
     * Returns the name of this product.
     *
     * @return Name of this product
     */
    const std::string& getName() const noexcept
    {
        return name;
    }

    /**
     * Accepts a chunk for incorporation.
     *
     * @param[in] chunk     Chunk to be incorporated
     * @retval    `true`    Chunk was incorporated
     * @retval    `false`   Chunk was previously incorporated. `log()` called.
     * @threadsafety        Safe
     * @exceptionsafety     Strong guarantee
     * @cancellationpoint   No
     */
    bool accept(Chunk& chunk)
    {
        auto index(chunk.getSegIndex());

        if (index >= numSegs)
            throw INVALID_ARGUMENT("Too large segment index: index: " +
                    std::to_string(index) + ", numSegs: " +
                    std::to_string(numSegs));

        if (segLedger[index])
            return false;

        auto size = chunk.getSize();
        auto expected = (index == numSegs - 1) ? lastSegSize : segSize;

        if (size != expected)
            throw INVALID_ARGUMENT("Unexpected segment size: expected: " +
                    std::to_string(expected) + ", actual: " +
                    std::to_string(size));

        chunk.write(data + index*segSize);
        segLedger.at(index) = true;
        ++numAccepted;

        return true;
    }

    /**
     * Indicates if this instance is complete (i.e., `accept()` has been called
     * for all segments).
     *
     * @retval `true`   Instance is complete
     * @retval `false`  Instance is not complete
     */
    bool isComplete() const noexcept
    {
        return numAccepted == numSegs;
    }

    /**
     * Writes this data-product to a file descriptor.
     *
     * @param[in] fd       File descriptor
     * @threadsafety       Safe
     * @exceptionsafety    Basic guarantee
     * @cancellationpoint  Yes
     */
    void write(int fd) const
    {
        if (::write(fd, data, prodSize) == -1)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(prodSize) +
                    " bytes of product \"" + name + "\" to file-descriptor " +
                    std::to_string(fd));
    }
};

MemProd::MemProd(Impl* const impl)
    : pImpl{impl}
{}

MemProd::MemProd(
        const std::string& name,
        const size_t       size,
        const ChunkSize    segSize)
    : pImpl{new Impl(name, size, segSize)}
{}

const std::string& MemProd::getName() const noexcept
{
    return pImpl->getName();
}

bool MemProd::accept(Chunk& chunk) const
{
    return pImpl->accept(chunk);
}

bool MemProd::isComplete() const noexcept
{
    return pImpl->isComplete();
}

void MemProd::write(int fd) const
{
    return pImpl->write(fd);
}

} // namespace
