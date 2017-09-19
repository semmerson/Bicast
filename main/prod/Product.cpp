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
#include "error.h"
#include "ProdInfo.h"
#include "Product.h"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace hycast {

class Product::Impl final
{
    ProdInfo          prodInfo;
    std::vector<bool> chunkVec;
    char*             data;
    ChunkIndex        numChunks;
    bool              complete;

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
     * Default constructs.
     */
    Impl()
        : prodInfo{}
        , chunkVec{}
        , data{nullptr}
        , numChunks{0}
        , complete{true}
    {}

    /**
     * Constructs from information on a product.
     * @param[in] info Information on a product
     */
    explicit Impl(const ProdInfo& prodInfo)
        : prodInfo{prodInfo}
        // `haveChunk{n}` means add `n` rather than have `n` elements
        , chunkVec(prodInfo.getNumChunks())
        , data{new char[prodInfo.getSize()]}
        , numChunks{0}
        , complete{false}
    {}

    /**
     * Constructs from complete data.
     * @param[in] name  Name of the product
     * @param[in] index  Product index
     * @param[in] data   Product data. Copied.
     * @param[in] size   Amount of data in bytes
     */
    Impl(   const std::string& name,
            const ProdIndex    index,
            const void*        data,
            const size_t       size)
        : Impl(ProdInfo(name, index, size))
    {
        numChunks = prodInfo.getNumChunks();
        complete = true;
        ::memcpy(this->data, data, size);
    }

    /**
     * Prevents copy and move construction.
     */
    Impl(const Impl& that) =delete;
    Impl(const Impl&& that) =delete;

    /**
     * Destroys this instance.
     */
    ~Impl()
    {
        delete[] data;
    }

    /**
     * Prevents copy and move assignment.
     */
    Impl& operator=(const Impl& rhs) =delete;
    Impl& operator=(const Impl&& rhs) =delete;

    /**
     * Sets the associated product-information providing it is consistent with
     * the information provided during construction (basically, only the name
     * can be changed).
     * @param[in] info       New product-information
     * @throw RuntimeError  `info` is inconsistent with existing information
     */
    void set(const ProdInfo& info)
    {
        if (info.getIndex() != prodInfo.getIndex() ||
                info.getSize() != prodInfo.getSize() ||
                info.getChunkSize() != prodInfo.getChunkSize())
            throw RUNTIME_ERROR(
                    "Replacement product-information is inconsistent: curr=" +
                    prodInfo.to_string() + ", new=" + info.to_string());
        prodInfo = info;
    }

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
     * Returns the product's index.
     * @return          Product's index
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    const ProdIndex getIndex() const noexcept
    {
        return prodInfo.getIndex();
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
        const auto chunkSize = chunk.getSize();
        prodInfo.vet(chunk.getInfo(), chunkSize);
        ChunkIndex chunkIndex{chunk.getInfo().getIndex()};
        if (complete || chunkVec[chunkIndex])
            return false;
        ::memcpy(startOf(chunkIndex), chunk.getData(), chunkSize);
        complete = ++numChunks == prodInfo.getNumChunks();
        return chunkVec[chunkIndex] = true;
    }

    /**
     * Adds a latent chunk-of-data.
     * @param[in] chunk      The latent chunk. `chunk.hasData()` will return
     *                       `false` on return.
     * @retval `true`        The chunk of data was added
     * @retval `false`       The chunk of data had already been added. This
     *                       instance is unchanged. The data in `chunk` has been
     *                       discarded.
     * @throws RuntimeError  `chunk` is inconsistent with this instance
     * @throws SystemError    An I/O error occurred
     * @execptionsafety      Strong guarantee
     * @threadsafety         Compatible but not safe
     */
    bool add(LatentChunk& chunk)
    {
        const auto chunkInfo = chunk.getInfo();
        const auto chunkIndex = chunkInfo.getIndex();
        if (complete || chunkVec[chunkIndex]) {
            chunk.discard();
            return false;
        }
        const auto expectedChunkSize = prodInfo.getChunkSize(chunkIndex);
        const auto chunkOffset = chunkInfo.getOffset();
        if (chunkOffset + expectedChunkSize > prodInfo.getSize()) {
            chunk.discard();
            throw RUNTIME_ERROR(
                    "Chunk offset + chunk size > product size: offset=" +
                    std::to_string(chunkOffset) + ", chunkSize=" +
                    std::to_string(expectedChunkSize) + ", prodSize=" +
                    std::to_string(prodInfo.getSize()));
        }
        const auto actualChunkSize = chunk.drainData(data+chunkOffset,
                expectedChunkSize);
        if (actualChunkSize != expectedChunkSize)
            throw RUNTIME_ERROR(
                    "Unexpected chunk size: expected=" +
                    std::to_string(expectedChunkSize) +
                    ", actual=" + std::to_string(actualChunkSize));
        complete = ++numChunks == prodInfo.getNumChunks();
        return chunkVec[chunkIndex] = true;
    }

    /**
     * Indicates if this instance is complete (i.e., contains all
     * chunks-of-data).
     * @return `true` iff this instance is complete
     */
    bool isComplete() const
    {
        return complete;
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

    bool operator ==(const Impl& that) const
    {
        return prodInfo == that.prodInfo &&
                ::memcmp(data, that.data, prodInfo.getSize()) == 0;
    }

    /**
     * Indicates if this instance contains a given chunk of data.
     * @param[in] index  Chunk index
     * @retval `true`    Chunk exists
     * @retval `false`   Chunk doesn't exist
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    bool haveChunk(const ChunkIndex index) const
    {
        if (index >= chunkVec.size())
            throw OUT_OF_RANGE("Chunk-index is too great: index=" +
                    std::to_string(index) + ", max=" +
                    std::to_string(chunkVec.size()-1));
        return complete || chunkVec[index];
    }

    /**
     * Returns the chunk of data corresponding to a chunk index.
     * @param[in]  index  Chunk index
     * @param[out] chunk  Corresponding chunk of data
     * @retval `true`     Chunk exists. `chunk` is set.
     * @retval `false`    Chunk doesn't exist. `chunk` isn't set.
     * @exceptionsafety   Strong guarantee
     * @threadsafety      Compatible but not safe
     */
    bool getChunk(
            const ChunkIndex index,
            ActualChunk&     chunk) const
    {
        if (!complete && !chunkVec[index])
            return false;
        auto info = prodInfo.makeChunkInfo(index);
        chunk = ActualChunk(info, data + prodInfo.getOffset(index));
        return true;
    }
};

Product::Product()
    : pImpl{new Impl()}
{}

Product::Product(const ProdInfo& info)
    : pImpl{new Impl(info)}
{}

Product::Product(
        const std::string& name,
        const ProdIndex    index,
        const void*        data,
        const size_t       size)
    : pImpl{new Impl(name, index, data, size)}
{
}

void Product::set(const ProdInfo& info)
{
    pImpl->set(info);
}

const ProdInfo& Product::getInfo() const noexcept
{
    return pImpl->getInfo();
}

const ProdIndex Product::getIndex() const noexcept
{
    return pImpl->getIndex();
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

bool Product::operator ==(const Product& that) const
{
    return *pImpl == *that.pImpl;
}

bool Product::haveChunk(const ChunkIndex index) const
{
    return pImpl->haveChunk(index);
}

bool Product::getChunk(
        const ChunkIndex index,
        ActualChunk&     chunk) const
{
    return pImpl->getChunk(index, chunk);
}

} // namespace
