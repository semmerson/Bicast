/**
 * This file implements a store of data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdStore.cpp
 * @author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "FixedDelayQueue.h"
#include "ProdStore.h"
#include "Product.h"

#include <cstdio>
#include <fstream>
#include <mutex>
#include <pthread.h>
#include <thread>
#include <unordered_map>

namespace hycast {

class ProdStore::Impl final
{
    /// Pathname of persistence file
    std::string                                pathname;
    /// Pathname of temporary persistence file
    std::string                                tempPathname;
    /// Persistence file
    std::ofstream                              file;
    /// Map of products
    std::unordered_map<ProdIndex, Product>     prods;
    /// Concurrent-access mutex
    std::mutex mutable                         mutex;
    /// Type of unit of residence-time
    typedef std::chrono::milliseconds          Duration;
    /// Product-deletion delay-queue
    FixedDelayQueue<ProdIndex, Duration>       delayQ;
    /// Thread for deleting products whose residence-time exceeds the minimum
    std::thread                                deletionThread;

    /**
     * Writes to the temporary persistence-file.
     */
    void writeTempFile()
    {
    }

    /**
     * Closes the temporary persistence-file.
     */
    void closeTempFile()
    {
        try {
            file.close();
        }
        catch (std::exception& e) {
            throw SystemError(__FILE__, __LINE__,
                    "Couldn't close temporary output product-store \"" +
                    tempPathname + "\"");
        }
    }

    /**
     * Renames the temporary persistence-file to the persistence-file.
     */
    void renameTempFile()
    {
        if (::rename(tempPathname.data(), pathname.data()))
            throw SystemError(__FILE__, __LINE__,
                    "Couldn't rename temporary output product-store \"" +
                    tempPathname + "\" to \"" + pathname + "\"");
    }

    /**
     * Deletes the temporary persistence-file.
     */
    void deleteTempFile()
    {
        if (::remove(tempPathname.data()))
            throw SystemError(__FILE__, __LINE__,
                    "Couldn't remove temporary output product-store \"" +
                    tempPathname);
    }

    /**
     * Saves the state of this instance in a persistence-file.
     */
    void persist()
    {
        try {
            writeTempFile();
            closeTempFile();
        }
        catch (std::exception& e1) {
            try {
                deleteTempFile();
            }
            catch (std::exception& e2) {
                log_what(e1);
                throw;
            }
            throw;
        }
        renameTempFile();
    }

    /**
     * Deletes products whose residence-time is greater than the minimum.
     * Doesn't return.
     */
    void deleteOldProds()
    {
        for (;;) {
            auto                             prodIndex = delayQ.pop();
            std::lock_guard<decltype(mutex)> lock(mutex);
            prods.erase(prodIndex);
        }
    }

public:
    /**
     * Constructs.
     * @param[in] pathname   Pathname of the persistence-file or the empty
     *                       string to indicate no persistence.
     * @param[in] residence  Minimum residence-time for a product in the store
     */
    Impl(   const std::string& pathname,
            const double       residence)
        : pathname{pathname}
        , tempPathname{pathname + ".tmp"}
        , file{}
        , prods{}
        , mutex{}
        , delayQ{Duration(static_cast<Duration::rep>(residence*1000))}
        , deletionThread{std::thread([this]{deleteOldProds();})}
    {
        if (pathname.length()) {
            file.open(tempPathname, std::ofstream::binary |
                    std::ofstream::trunc);
            if (file.fail())
                throw SystemError(__FILE__, __LINE__,
                        "Couldn't open temporary output product-store \"" +
                        tempPathname + "\"");
        }
        if (residence < 0)
            throw InvalidArgument(__FILE__, __LINE__,
                    "Residence-time is negative: " + std::to_string(residence));
    }

    /**
     * Destroys. Saves the state of this instance in the persistence-file if
     * one was specified during construction.
     */
    ~Impl()
    {
        try {
            if (file.is_open())
                persist();
        }
        catch (const std::exception& e) {
            log_what(e);
        }
        try {
            ::pthread_cancel(deletionThread.native_handle());
            deletionThread.join();
        }
        catch (const std::exception& e) {
            log_what(e);
        }
    }

    /**
     * Adds information on a product.
     * @param[in]  prodInfo  Product information
     * @param[out] prod      Associated product
     * @retval `true`        The product is complete
     * @retval `false`       The product is incomplete
     */
    bool add(
            const ProdInfo& prodInfo,
            Product&        prod)
    {
        auto                              prodIndex = prodInfo.getIndex();
        std::lock_guard<decltype(mutex)>  lock(mutex);
        auto                              iter = prods.find(prodIndex);
        if (iter != prods.end()) {
            prod = iter->second;
            prod.set(prodInfo);
        }
        else {
            prod = Product(prodInfo);
            prods.emplace(prodIndex, prod);
            delayQ.push(prodIndex);
        }
        return prod.isComplete();
    }

    /**
     * Adds a chunk of data.
     * @param[in]  chunk  Chunk of data
     * @param[out] prod   Associated product
     * @retval `true`     The product is complete
     * @retval `false`    The product is incomplete
     */
    bool add(
            LatentChunk& chunk,
            Product&     prod)
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        auto                             prodIndex = chunk.getProdIndex();
        auto                             iter = prods.find(prodIndex);
        if (iter != prods.end()) {
            prod = iter->second;
            prod.add(chunk);
            return prod.isComplete() && prod.getInfo().getName().length() > 0;
        }
        prod = Product(ProdInfo("", prodIndex, chunk.getProdSize()));
        prods.emplace(prodIndex, prod);
        prod.add(chunk);
        delayQ.push(prodIndex);
        return false;
    }

    /**
     * Returns the number of products in the store -- both complete and
     * incomplete.
     * @return Number of products in the store
     */
    size_t size() const noexcept
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        return prods.size();
    }

    /**
     * Returns product-information on a given data-product.
     * @param[in]  index  Index of the data-product
     * @param[out] info   Information on the given product
     * @retval `true`     Information found. `info` is set.
     * @retval `false`    Information not found. `info` is not set.
     */
    bool getProdInfo(
            const ProdIndex index,
            ProdInfo&       info) const
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        auto                             iter = prods.find(index);
        if (iter == prods.end())
            return false;
        info = iter->second.getInfo();
        return true;
    }

    /**
     * Indicates if this instance contains a given chunk of data.
     * @param[in] info  Information on the chunk
     * @retval `true`   Chunk exists
     * @retval `false`  Chunk doesn't exist
     */
    bool haveChunk(const ChunkInfo& info) const
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        auto                             iter = prods.find(info.getProdIndex());
        if (iter == prods.end())
            return false;
        auto prod = iter->second;
        return prod.haveChunk(info.getIndex());
    }

    /**
     * Returns the chunk of data corresponding to chunk-information.
     * @param[in]  info   Information on the desired chunk
     * @param[out] chunk  Corresponding chunk of data
     * @retval `true`     Chunk found. `chunk` is set.
     * @retval `false`    Chunk not found. `chunk` is not set.
     */
    bool getChunk(
            const ChunkInfo& info,
            ActualChunk&     chunk) const
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        auto                             iter = prods.find(info.getProdIndex());
        if (iter == prods.end())
            return false;
        auto prod = iter->second;
        prod.getChunk(info.getIndex(), chunk);
        return true;
    }
};

ProdStore::ProdStore(
        const std::string& pathname,
        const double       residence)
    : pImpl{new Impl(pathname, residence)}
{}

bool ProdStore::add(
        const ProdInfo& prodInfo,
        Product&        prod)
{
    return pImpl->add(prodInfo, prod);
}

bool ProdStore::add(
        LatentChunk& chunk,
        Product&     prod)
{
    return pImpl->add(chunk, prod);
}

size_t ProdStore::size() const noexcept
{
    return pImpl->size();
}

bool ProdStore::getProdInfo(
        const ProdIndex index,
        ProdInfo&       info) const
{
    return pImpl->getProdInfo(index, info);
}

bool ProdStore::haveChunk(const ChunkInfo& info) const
{
    return pImpl->haveChunk(info);
}

bool ProdStore::getChunk(
        const ChunkInfo& info,
        ActualChunk&     chunk) const
{
    return pImpl->getChunk(info, chunk);
}

} // namespace
