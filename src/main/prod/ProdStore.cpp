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

    void writeTempFile()
    {
    }

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

    void renameTempFile()
    {
        if (::rename(tempPathname.data(), pathname.data()))
            throw SystemError(__FILE__, __LINE__,
                    "Couldn't rename temporary output product-store \"" +
                    tempPathname + "\" to \"" + pathname + "\"");
    }

    void deleteTempFile()
    {
        if (::remove(tempPathname.data()))
            throw SystemError(__FILE__, __LINE__,
                    "Couldn't remove temporary output product-store \"" +
                    tempPathname);
    }

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

    void deleteOldProds()
    {
        for (;;) {
            auto prodIndex = delayQ.pop();
            std::unique_lock<decltype(mutex)> lock(mutex);
            prods.erase(prodIndex);
        }
    }

public:
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

    bool add(
            const ProdInfo& prodInfo,
            Product&        prod)
    {
        auto                              prodIndex = prodInfo.getIndex();
        std::unique_lock<decltype(mutex)> lock(mutex);
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

    bool add(LatentChunk& chunk, Product& prod)
    {
        std::unique_lock<decltype(mutex)> lock(mutex);
        auto prodIndex = chunk.getProdIndex();
        auto iter = prods.find(prodIndex);
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
        std::unique_lock<decltype(mutex)> lock(mutex);
        return prods.size();
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

bool ProdStore::add(LatentChunk& chunk, Product& prod)
{
    return pImpl->add(chunk, prod);
}

size_t ProdStore::size() const noexcept
{
    return pImpl->size();
}

} // namespace
