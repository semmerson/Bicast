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

#include <atomic>
#include <cstdio>
#include <exception>
#include <fstream>
#include <mutex>
#include <pthread.h>
#include <thread>
#include <unordered_map>

namespace hycast {

class ProdStore::Impl final
{
    /// Product entry
    class ProdEntry
    {
        bool    processed;
        Product prod;

    public:
        ProdEntry(const ProdInfo& prodInfo)
            : processed{false}
            , prod{prodInfo}
        {}

        ProdEntry(ProdInfo&& prodInfo)
            : processed{false}
            , prod{prodInfo}
        {}

        ProdEntry(LatentChunk& chunk)
            : ProdEntry{ProdInfo{"", chunk.getProdIndex(), chunk.getProdSize()}}
        {
            prod.add(chunk);
        }

        ProdEntry(Product& prod)
            : processed{false}
            , prod{prod}
        {}

        inline bool isEarlierThan(ProdEntry& that)
        {
            return prod.isEarlierThan(that.prod);
        }

        inline ChunkInfo identifyEarliestMissingChunk() const
        {
            return prod.identifyEarliestMissingChunk();
        }

        inline void set(const ProdInfo& prodInfo)
        {
            prod.set(prodInfo);
        }

        inline void add(LatentChunk& chunk)
        {
            prod.add(chunk);
        }

        inline const ProdInfo& getInfo() const
        {
            return prod.getInfo();
        }

        inline bool isReady() const
        {
            return !processed && prod.isComplete() &&
                    prod.getInfo().getName().length() > 0;
        }

        const inline Product& getProduct() const
        {
            return prod;
        }

        inline void setProcessed()
        {
            processed = true;
        }

        inline bool haveChunk(ChunkIndex index) const
        {
            return prod.haveChunk(index);
        }

        inline bool getChunk(ChunkIndex index, ActualChunk& chunk) const
        {
            return prod.getChunk(index, chunk);
        }
    };

    typedef std::mutex                 Mutex;
    typedef std::lock_guard<Mutex>     LockGuard;
    typedef std::chrono::milliseconds  Duration; /// Unit of residence-time

    /// Pathname of persistence file
    std::string                                pathname;
    /// Pathname of temporary persistence file
    std::string                                tempPathname;
    /// Persistence file
    std::ofstream                              file;
    /// Map of products
    std::unordered_map<ProdIndex, ProdEntry>   prods;
    /// Map of pending (i.e., incomplete) products
    std::map<ProdIndex, ProdEntry*>            pending;
    /// Concurrent-access mutex
    mutable Mutex                              mutex;
    /// Product-deletion delay-queue
    FixedDelayQueue<ProdIndex, Duration>       delayQ;
    /// Thread for deleting products whose residence-time exceeds the minimum
    std::thread                                deletionThread;
    std::exception_ptr                         exception;

    /**
     * Writes to the temporary persistence-file.
     */
    void writeTempFile()
    {
        throw LOGIC_ERROR("Not implemented yet");
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
            throw SYSTEM_ERROR(
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
            throw SYSTEM_ERROR(
                    "Couldn't rename temporary output product-store \"" +
                    tempPathname + "\" to \"" + pathname + "\"");
    }

    /**
     * Deletes the temporary persistence-file.
     */
    void deleteTempFile()
    {
        if (::remove(tempPathname.data()))
            throw SYSTEM_ERROR(
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
                log_error(e1);
                throw;
            }
            throw;
        }
        renameTempFile();
    }

    /**
     * Deletes products whose residence-time is greater than the minimum.
     * Doesn't return. Intended to run on its own thread.
     */
    void deleteOldProds()
    {
    	try {
            for (;;) {
                auto      prodIndex = delayQ.pop();
                LockGuard lock{mutex};
                prods.erase(prodIndex);
                pending.erase(prodIndex);
            }
    	}
    	catch (const std::exception& e) {
            LockGuard lock{mutex};
            exception = std::current_exception();
    	}
    }

public:
    /**
     * Constructs.
     * @param[in] pathname   Pathname of the persistence-file or the empty
     *                       string to indicate no persistence.
     * @param[in] residence  Minimum residence-time for a product in the store
     *                       in seconds
     */
    Impl(   const std::string& pathname,
            const double       residence)
        : pathname{pathname}
        , tempPathname{pathname + ".tmp"}
        , file{}
        , prods{}
        , pending{}
        , mutex{}
        , delayQ{Duration(static_cast<Duration::rep>(residence*1000))}
        , deletionThread{std::thread([this]{deleteOldProds();})}
        , exception{}
    {
        if (pathname.length()) {
            file.open(tempPathname, std::ofstream::binary |
                    std::ofstream::trunc);
            if (file.fail())
                throw SYSTEM_ERROR(
                        "Couldn't open temporary output product-store \"" +
                        tempPathname + "\"");
        }
        if (residence < 0)
            throw INVALID_ARGUMENT("Residence-time is negative: " +
                    std::to_string(residence));
    }

    /**
     * Destroys. Saves the state of this instance in the persistence-file if
     * one was specified during construction.
     */
    ~Impl() noexcept
    {
        try {
            if (file.is_open())
                persist();
        }
        catch (const std::exception& e) {
            log_error(e);
        }
        try {
            ::pthread_cancel(deletionThread.native_handle());
            deletionThread.join();
        }
        catch (const std::exception& e) {
            log_error(e);
        }
    }

    /**
     * Adds an entire product. Does nothing if the product has already been
     * added. If added, the product will be removed when the minimum residence
     * time has elapsed.
     * @param[in] prod   Product to be added
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    void add(Product& prod)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        auto prodIndex = prod.getIndex();
        auto pair = prods.insert(std::pair<ProdIndex, ProdEntry>
                (prodIndex, ProdEntry{prod}));
        if (pair.second)
            delayQ.push(prodIndex);
    }

    /**
     * Adds information on a product. If the addition completes the product,
     * then it will be removed from the map of pending products and removed from
     * the map of products when the minimum residence time has elapsed.
     * @param[in]  prodInfo  Product information
     * @param[out] prod      Associated product
     * @retval `true`        The product is complete
     * @retval `false`       The product is incomplete
     */
    bool add(
            const ProdInfo& prodInfo,
            Product&        prod)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        ProdEntry* entry;
        auto prodIndex = prodInfo.getIndex();
        auto iter = prods.find(prodIndex);
        if (iter == prods.end()) {
            entry = &prods.emplace(prodIndex, ProdEntry{prodInfo}).first->second;
            pending[prodIndex] = entry;
        }
        else {
            entry = &iter->second;
            entry->set(prodInfo);
        }
        prod = entry->getProduct();
        delayQ.push(prodIndex);
        if (!entry->isReady())
            return false;
        entry->setProcessed();
        pending.erase(prodIndex);
        return true;
    }

    /**
     * Adds a chunk of data. If the addition completes the product, then it will
     * be removed when the minimum residence time has elapsed.
     * @param[in]  chunk  Chunk of data
     * @param[out] prod   Associated product
     * @retval `true`     The product is complete
     * @retval `false`    The product is incomplete
     */
    bool add(
            LatentChunk& chunk,
            Product&     prod)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
    	ProdEntry* entry;
        auto prodIndex = chunk.getProdIndex();
        auto iter = prods.find(prodIndex);
        if (iter == prods.end()) {
            entry = &prods.emplace(prodIndex, ProdEntry{chunk}).first->second;
            pending[prodIndex] = entry;
        }
        else {
            entry = &iter->second;
            entry->add(chunk);
        }
        prod = entry->getProduct();
        delayQ.push(prodIndex);
        if (!entry->isReady())
            return false;
        entry->setProcessed();
        pending.erase(prodIndex);
        return true;
    }

    /**
     * Returns the number of products in the store -- both complete and
     * incomplete.
     * @return Number of products in the store
     */
    size_t size() const noexcept
    {
        LockGuard lock{mutex};
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
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        auto iter = prods.find(index);
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
        LockGuard lock{mutex};
        auto      iter = prods.find(info.getProdIndex());
        if (iter == prods.end())
            return false;
        return iter->second.haveChunk(info.getIndex());
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
        LockGuard lock{mutex};
        auto      iter = prods.find(info.getProdIndex());
        if (iter == prods.end())
            return false;
        return iter->second.getChunk(info.getIndex(), chunk);
    }

    /**
     * Returns information on the earliest missing chunk of data.
     * @return  Information on the earliest missing data-chunk or empty
     *          information if no such chunk exists
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     * @see `ChunkInfo::operator bool()`
     */
    ChunkInfo identifyEarliestMissingChunk() const
    {
        LockGuard lock{mutex};
        auto iter = pending.begin();
        return (iter == pending.end())
                ? ChunkInfo{}
                : iter->second->identifyEarliestMissingChunk();
    }
};

ProdStore::ProdStore(
        const std::string& pathname,
        const double       residence)
    : pImpl{new Impl(pathname, residence)}
{}

void ProdStore::add(Product& prod)
{
    pImpl->add(prod);
}

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

ChunkInfo ProdStore::getOldestMissingChunk() const
{
    return pImpl->identifyEarliestMissingChunk();
}

} // namespace
