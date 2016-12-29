/**
 * This file implements a data-product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Product.cpp
 * @author: Steven R. Emmerson
 */

#include "Chunk.h"
#include "ProdInfo.h"
#include "Product.h"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace hycast {

class ProductImpl final
{
    ProdInfo          prodInfo;
    std::vector<bool> haveChunk;
    char*             data;
    ChunkIndex        numChunks;

    /**
     * Returns a pointer to the start of a chunk-of-data in the accumulating
     * buffer.
     * @param chunkIndex  Index of the chunk
     * @return            Pointer to the start of the chunk
     */
    char* startOf(const ChunkIndex chunkIndex) const
    {
        return data + chunkIndex * prodInfo.getChunkSize();
    }

public:
    /**
     * Constructs from information on a product.
     * @param[in] info Information on a product
     */
    explicit ProductImpl(const ProdInfo& prodInfo)
        : prodInfo{prodInfo}
        // `haveChunk{n}` means add `n` rather than have `n` elements
        , haveChunk(prodInfo.getNumChunks())
        , data{new char[prodInfo.getSize()]}
        , numChunks{0}
    {}

    /**
     * Returns information on the product.
     * @return Information on the product
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const ProdInfo& getInfo() const noexcept
    {
        return prodInfo;
    }

    /**
     * Destroys this instance.
     */
    ~ProductImpl()
    {
        delete[] data;
    }

    /**
     * Adds a chunk-of-data.
     * @param[in] chunk  The chunk
     * @return `true`    if the chunk of data was added
     * @return `false`   if the chunk of data had already been added. The
     *                   instance is unchanged.
     * @throws std::invalid_argument if the chunk is inconsistent with this
     *                               instance
     * @execptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    bool add(const ActualChunk& chunk)
    {
        ChunkSize chunkSize{chunk.getSize()};
        prodInfo.vet(chunk.getInfo(), chunkSize);
        ChunkIndex chunkIndex{chunk.getInfo().getChunkIndex()};
        if (haveChunk[chunkIndex])
            return false;
        ::memcpy(startOf(chunkIndex), chunk.getData(), chunkSize);
        ++numChunks;
        return haveChunk[chunkIndex] = true;
    }

    /**
     * Adds a latent chunk-of-data.
     * @param[in] chunk  The latent chunk
     * @return `true`    if the chunk of data was added
     * @return `false`   if the chunk of data had already been added. The
     *                   instance is unchanged.
     * @throws std::invalid_argument if the chunk is inconsistent with this
     *                               instance
     * @throws std::system_error     if an I/O error occurs
     * @execptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    bool add(LatentChunk& chunk)
    {
        prodInfo.vet(chunk.getInfo(), chunk.getSize());
        ChunkIndex chunkIndex{chunk.getInfo().getChunkIndex()};
        if (haveChunk[chunkIndex])
            return false;
        chunk.drainData(startOf(chunkIndex));
        ++numChunks;
        return haveChunk[chunkIndex] = true;
    }

    /**
     * Indicates if this instance is complete (i.e., contains all
     * chunks-of-data).
     * @return `true` iff this instance is complete
     */
    bool isComplete() const
    {
        return numChunks == prodInfo.getNumChunks();
    }

    /**
     * Returns a pointer to the data.
     * @return a pointer to the data
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const char* getData() const noexcept
    {
        return data;
    }
};

Product::Product(const ProdInfo& info)
    : pImpl{new ProductImpl(info)}
{}

const ProdInfo& hycast::Product::getInfo() const noexcept
{
    return pImpl->getInfo();
}

bool Product::add(const ActualChunk& chunk)
{
    return pImpl->add(chunk);
}

bool Product::add(LatentChunk& chunk)
{
    return pImpl->add(chunk);
}

bool Product::isComplete() const
{
    return pImpl->isComplete();
}

const char* Product::getData() const noexcept
{
    return pImpl->getData();
}

} // namespace
