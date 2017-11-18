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
#include <fcntl.h>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace hycast {

class Product::Impl
{
protected:
    ProdInfo prodInfo;

    /**
     * Default constructs.
     */
    Impl()
        : prodInfo{}
    {}

    /**
     * Constructs from information on a product.
     * @param[in] info Information on a product
     */
    explicit Impl(const ProdInfo& prodInfo)
        : prodInfo{prodInfo}
    {}

public:
    virtual ~Impl()
    {}

    /**
     * Prevents copy and move construction.
     */
    Impl(const Impl& that) =delete;
    Impl(const Impl&& that) =delete;

    /**
     * Prevents copy and move assignment.
     */
    Impl& operator=(const Impl& rhs) =delete;
    Impl& operator=(const Impl&& rhs) =delete;

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
     * Indicates if this instance is earlier than another.
     * @param[in] that   Other instance
     * @retval `true`    Yes
     * @retval `false`   No
     */
    bool isEarlierThan(const Impl& that) const noexcept
    {
        return prodInfo.isEarlierThan(that.prodInfo);
    }

    virtual bool haveChunk(const ChunkOffset index) const =0;

    virtual bool haveChunk(const ChunkIndex index) const =0;

    virtual ActualChunk getChunk(const ChunkIndex index) const =0;

    virtual ChunkId identifyEarliestMissingChunk() const noexcept =0;

    virtual bool set(const ProdInfo& info) =0;

    virtual bool add(const ActualChunk& chunk) =0;

    virtual bool add(LatentChunk& chunk) =0;

    virtual bool isComplete() const noexcept =0;

    virtual const char* getData() const noexcept =0;
};

Product::Product(Impl* ptr)
    : pImpl{ptr}
{}

Product::~Product()
{}

bool Product::set(const ProdInfo& info)
{
    return pImpl->set(info);
}

const ProdInfo& Product::getInfo() const noexcept
{
    return pImpl->getInfo();
}

const ProdIndex Product::getIndex() const noexcept
{
    return pImpl->getIndex();
}

bool Product::isEarlierThan(const Product& that) const noexcept
{
    return pImpl->isEarlierThan(*that.pImpl.get());
}

ChunkId Product::identifyEarliestMissingChunk() const noexcept
{
    return pImpl->identifyEarliestMissingChunk();
}

bool Product::add(const ActualChunk& chunk)
{
    return pImpl->add(chunk);
}

bool Product::add(LatentChunk& chunk)
{
    return pImpl->add(chunk);
}

bool Product::isComplete() const noexcept
{
    return pImpl->isComplete();
}

const char* Product::getData() const noexcept
{
    return pImpl->getData();
}

bool Product::haveChunk(const ChunkOffset offset) const
{
    return pImpl->haveChunk(offset);
}

bool Product::haveChunk(const ChunkIndex index) const
{
    return pImpl->haveChunk(index);
}

ActualChunk Product::getChunk(const ChunkIndex index) const
{
    return pImpl->getChunk(index);
}

/******************************************************************************/

class CompleteProduct::Impl : public Product::Impl
{
    const char* data;

public:
    Impl(   const ProdIndex    index,
            const std::string& name,
            const ProdSize     size,
            const char*        data,
            const ChunkSize    chunkSize)
        : Product::Impl{ProdInfo{index, name, size, chunkSize}}
        , data{data}
    {}

    ChunkId identifyEarliestMissingChunk() const noexcept
    {
        throw LOGIC_ERROR(
                "Complete product doesn't have earliest missing chunk");
    }

    bool haveChunk(const ChunkOffset offset) const
    {
        return offset < prodInfo.getSize();
    }

    bool haveChunk(const ChunkIndex index) const
    {
        return haveChunk(prodInfo.getChunkOffset(index));
    }

    ActualChunk getChunk(const ChunkIndex index) const
    {
        auto offset = prodInfo.getChunkOffset(index);
        return ActualChunk{prodInfo, index, data + offset};
    }

    /**
     * Sets the associated product-information providing it is consistent with
     * the information provided during construction (basically, only the name
     * can be changed).
     * @param[in] info       New product-information
     * @retval `false`       Duplicate information
     * @retval `true`        New information consistent with existing
     * @throw LogicError     Product information can't be changed
     */
    bool set(const ProdInfo& info)
    {
        throw LOGIC_ERROR(
                "Can't set product-information of complete product");
    }

    bool add(const ActualChunk& chunk)
    {
        throw LOGIC_ERROR("Can't add data to complete product");
    }

    bool add(LatentChunk& chunk)
    {
        throw LOGIC_ERROR("Can't add data to complete product");
    }

    /**
     * Indicates if this instance is complete (i.e., contains all
     * chunks-of-data).
     * @return `true` iff this instance is complete
     */
    bool isComplete() const noexcept
    {
        return true;
    }

    const char* getData() const noexcept
    {
        return data;
    }
};


CompleteProduct::CompleteProduct()
    : Product{}
{}

CompleteProduct::CompleteProduct(
        const ProdIndex index,
        const ProdName& name,
        const ProdSize  size,
        const char*     data,
        const ChunkSize chunkSize)
    : Product{new Impl{index, name, size, data, chunkSize}}
{}

/******************************************************************************/

class PartialProduct::Impl : public Product::Impl
{
    std::vector<bool> chunkVec;
    char*             data;
    ChunkIndex::type  numChunks; /// Current number of contained chunks
    bool              complete;

public:
    explicit Impl(const ProdInfo& prodInfo)
        : Product::Impl{prodInfo}
        // Parentheses are necessary in the following initialization
        , chunkVec(prodInfo.getNumChunks(), false)
        , data{new char[prodInfo.getSize()]}
        , numChunks{0}
        , complete{false}
    {}

    Impl(const Impl& that) =delete;
    Impl& operator=(const Impl& rhs) =delete;

    /**
     * Destroys this instance.
     */
    ~Impl()
    {
        delete[] data;
    }

    ChunkId identifyEarliestMissingChunk() const noexcept
    {
        if (!complete) {
            auto n = chunkVec.size();
            for (ChunkIndex::type i = 0; i < n; ++i) {
                if (!chunkVec[i])
                    // Won't throw exception because `chunkVec` set by `prodInfo`
                    return ChunkId{prodInfo, static_cast<ChunkIndex>(i)};
            }
        }
        return ChunkId{};
    }

    bool add(const ActualChunk& chunk)
    {
        if (complete)
            return false;
        auto chunkIndex = chunk.getIndex();
        if (chunkVec[chunkIndex]) // Safe because index vetted by `chunk`
            return false;
        auto chunkOffset = chunk.getOffset();
        ::memcpy(data + chunk.getOffset(), chunk.getData(), chunk.getSize());
        chunkVec[chunkIndex] = true;
        complete = ++numChunks == prodInfo.getNumChunks();
        if (complete)
            std::vector<bool>().swap(chunkVec); // clear by reallocating
        return true;
    }

    bool add(LatentChunk& chunk)
    {
        if (complete) {
            chunk.discard();
            return false;
        }
        if (chunk.getInfo() != prodInfo.getChunkInfo(chunk.getIndex())) {
            throw INVALID_ARGUMENT("Inconsistent latent-chunk information: "
                    "chunkInfo=" + std::to_string(chunk.getInfo()) +
                    ", expected=" +
                    std::to_string(prodInfo.getChunkInfo(chunk.getIndex())));
        }
        const auto chunkIndex = chunk.getIndex();
        if (chunkVec.at(chunkIndex)) {
            chunk.discard();
            return false;
        }
        const auto expectedChunkSize = prodInfo.getChunkSize(chunkIndex);
        const auto chunkOffset = chunk.getOffset();
        if (chunkOffset + expectedChunkSize > prodInfo.getSize()) {
            chunk.discard();
            throw RUNTIME_ERROR("chunkOffset{" + std::to_string(chunkOffset) +
                    "} + chunkSize{" + std::to_string(expectedChunkSize) +
                    "} > productSize{" + std::to_string(prodInfo.getSize()) +
                    "}");
        }
        const auto actualChunkSize = chunk.drainData(data+chunkOffset,
                expectedChunkSize);
        if (actualChunkSize != expectedChunkSize)
            throw RUNTIME_ERROR(
                    "Unexpected chunk size: expected=" +
                    std::to_string(expectedChunkSize) +
                    ", actual=" + std::to_string(actualChunkSize));
        complete = ++numChunks == prodInfo.getNumChunks();
        if (complete) {
            std::vector<bool>().swap(chunkVec); // clear by reallocating
        }
        else {
            chunkVec[chunkIndex] = true;
        }
        return true;
    }

    bool set(const ProdInfo& info)
    {
        if (info.getIndex() != prodInfo.getIndex() ||
                info.getSize() != prodInfo.getSize() ||
                info.getChunkSize() != prodInfo.getChunkSize())
            throw RUNTIME_ERROR(
                    "Replacement product-information is inconsistent: curr=" +
                    prodInfo.to_string() + ", new=" + info.to_string());
        const bool isNew = prodInfo.getName().to_string().length() == 0 &&
                info.getName().to_string().length() > 0;
        prodInfo = info;
        return isNew;
    }

    bool isComplete() const noexcept
    {
        return complete && prodInfo.getName().to_string().length() > 0;
    }

    bool haveChunk(const ChunkIndex index) const
    {
        return index < prodInfo.getNumChunks() && (complete || chunkVec[index]);
    }

    bool haveChunk(const ChunkOffset offset) const
    {
        return haveChunk(prodInfo.getChunkIndex(offset));
    }

    ActualChunk getChunk(const ChunkIndex index) const
    {
        auto offset = prodInfo.getChunkOffset(index); // Vets `index`
        return (complete || chunkVec[index])
                ? ActualChunk{prodInfo, index, data + offset}
                : ActualChunk{};
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
}; // `PartialProduct::Impl`

PartialProduct::PartialProduct()
    : Product{}
{}

PartialProduct::PartialProduct(const ProdInfo& prodInfo)
    : Product{new Impl{prodInfo}}
{}

} // namespace
