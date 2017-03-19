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
#include "ProdStore.h"
#include "Product.h"

#include <cstdio>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace hycast {

class ProdStore::Impl final
{
    std::string                            pathname;
    std::string                            tempPathname;
    std::ofstream                          file;
    std::unordered_map<ProdIndex, Product> prods;
    std::mutex                             mutex;

    void persist()
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

public:
    Impl(const std::string& pathname)
        : pathname{pathname}
        , tempPathname{pathname + ".tmp"}
        , file{}
        , prods{}
        , mutex{}
    {
        if (pathname.length()) {
            file.open(tempPathname, std::ofstream::binary |
                    std::ofstream::trunc);
            if (file.fail())
                throw SystemError(__FILE__, __LINE__,
                        "Couldn't open temporary output product-store \"" +
                        tempPathname + "\"");
        }
    }

    ~Impl()
    {
        if (file.is_open()) {
            try {
                persist();
                closeTempFile();
                try {
                    renameTempFile();
                }
                catch (std::exception& e) {
                    log_what(e);
                }
            }
            catch (std::exception& e) {
                log_what(e);
                try {
                    deleteTempFile();
                }
                catch (std::exception& e) {
                    log_what(e);
                }
            }
        }
    }

    bool add(const ProdInfo& prodInfo)
    {
        auto                              prodIndex = prodInfo.getIndex();
        std::unique_lock<decltype(mutex)> lock(mutex);
        auto                              iter = prods.find(prodIndex);
        if (iter != prods.end())
            return false;
        prods[prodIndex] = Product(prodInfo);
        return true;
    }

    bool add(LatentChunk& chunk, Product& prod)
    {
        std::unique_lock<decltype(mutex)> lock(mutex);
        prod = prods[chunk.getProdIndex()];
        return prod.add(chunk);
    }
};

ProdStore::ProdStore(const std::string& pathname)
    : pImpl{new Impl(pathname)}
{}

bool ProdStore::add(const ProdInfo& prodInfo)
{
    return pImpl->add(prodInfo);
}

bool ProdStore::add(LatentChunk& chunk, Product& prod)
{
    return pImpl->add(chunk, prod);
}

} // namespace
