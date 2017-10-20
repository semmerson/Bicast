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

/// Product entry
class ProdEntry
{
    Product prod;

public:
    ProdEntry(const ProdInfo& prodInfo)
        : prod{prodInfo}
    {}

    ProdEntry(ProdInfo&& prodInfo)
        : prod{prodInfo}
    {}

    ProdEntry(LatentChunk& chunk)
        : ProdEntry{ProdInfo{"", chunk.getProdIndex(), chunk.getProdSize()}}
    {
        prod.add(chunk);
    }

    ProdEntry(Product& prod)
        : prod{prod}
    {}

    inline bool isEarlierThan(ProdEntry& that)
    {
        return prod.isEarlierThan(that.prod);
    }

    inline ChunkInfo identifyEarliestMissingChunk() const
    {
        return prod.identifyEarliestMissingChunk();
    }

    inline bool set(const ProdInfo& prodInfo)
    {
        return prod.set(prodInfo);
    }

    inline bool add(LatentChunk& chunk)
    {
        return prod.add(chunk);
    }

    inline const ProdInfo& getInfo() const
    {
        return prod.getInfo();
    }

    inline bool isReady() const
    {
        return prod.isComplete() && (prod.getInfo().getName().length() > 0);
    }

    const inline Product& getProduct() const
    {
        return prod;
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

/******************************************************************************/

/// Map of products
class ProdMap
{
    typedef std::mutex             Mutex;
    typedef std::lock_guard<Mutex> LockGuard;

    mutable Mutex                            mutex;
    /// Map of products
    std::unordered_map<ProdIndex, ProdEntry> prods;
    /// Map of incomplete products
    std::map<ProdIndex, ProdEntry*>          incomplete;
    ProdIndex                                earliest;
    ProdIndex                                latest;
    static const ChunkInfo                   emptyChunkInfo;

    /**
     * Updates the indexes of the earliest and latest products that this
     * instance contains.
     * @pre `mutex` is locked
     * @param[in] index  Product index
     */
    void updateEndIndexes(const ProdIndex& index)
    {
        if (prods.size() == 0) {
            latest = earliest = index;
        }
        else {
            if (index < earliest)
                earliest = index;
            if (index > latest)
                latest = index;
        }
    }

public:
    ProdMap()
        : mutex{}
        , prods{}
        , incomplete{}
        , earliest{}
        , latest{}
    {}

    /**
     * Adds an entire product. Does nothing if the product has already been
     * added.
     * @param[in] prod   Product to be added
     * @retval `true`    Product is new
     * @retval `false`   Product is not new
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    bool add(Product& prod)
    {
        LockGuard  lock{mutex};
        auto       prodIndex = prod.getIndex();
        auto       pair = prods.insert(std::pair<ProdIndex, ProdEntry>
                (prodIndex, ProdEntry{prod}));
        const bool isNew = pair.second;
        if (isNew)
            updateEndIndexes(prodIndex);
        incomplete.erase(prodIndex); // Just to make sure
        return isNew;
    }

    /**
     * Adds information on a product.
     * @param[in]  prodInfo  Product information
     * @param[out] prod      Associated product
     * @return               Status of addition
     * @see                  `ProdStore::AddStatus`
     */
    ProdStore::AddStatus add(
            const ProdInfo&   prodInfo,
            ProdEntry** const prodEntry)
    {
        LockGuard             lock{mutex};
        ProdEntry*            entry;
        ProdStore::AddStatus  status{};
        const auto            prodIndex = prodInfo.getIndex();
        auto                  iter = prods.find(prodIndex);
        if (iter == prods.end()) {
            status.setNew();
            entry = &prods.emplace(prodIndex,
                    ProdEntry{prodInfo}).first->second;
            incomplete[prodIndex] = entry;
            updateEndIndexes(prodIndex);
        }
        else {
            entry = &iter->second;
            if (entry->set(prodInfo)) {
                status.setNew();
            }
            else {
                status.setDuplicate();
            }
            if (entry->isReady()) {
                status.setComplete();
                incomplete.erase(prodIndex);
            }
        }
        *prodEntry = entry;
        return status;
    }

    /**
     * Adds a chunk of data.
     * @param[in]  chunk  Chunk of data
     * @param[out] prod   Associated product
     * @return            Status of addition
     * @see               `ProdStore::AddStatus`
     */
    ProdStore::AddStatus add(
            LatentChunk&      chunk,
            ProdEntry** const prodEntry)
    {
        LockGuard            lock{mutex};
        ProdEntry*           entry;
        ProdStore::AddStatus status{};
        const auto           prodIndex = chunk.getProdIndex();
        auto                 iter = prods.find(prodIndex);
        if (iter == prods.end()) {
            status.setNew();
            entry = &prods.emplace(prodIndex, ProdEntry{chunk}).first->second;
            incomplete[prodIndex] = entry;
            updateEndIndexes(prodIndex);
        }
        else {
            entry = &iter->second;
            if (entry->add(chunk)) {
                status.setNew();
            }
            else {
                status.setDuplicate();
            }
            if (entry->isReady()) {
                status.setComplete();
                incomplete.erase(prodIndex);
            }
        }
        *prodEntry = entry;
        return status;
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

    ProdIndex getEarliest()
    {
        LockGuard lock{mutex};
        return earliest;
    }

    ProdIndex getLatest()
    {
        LockGuard lock{mutex};
        return latest;
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

    ChunkInfo identifyEarliestMissingChunk() const
    {
        LockGuard lock{mutex};
        auto iter = incomplete.begin();
        return (iter == incomplete.end())
                ? emptyChunkInfo
                : iter->second->identifyEarliestMissingChunk();
    }

    void erase(const ProdIndex& index)
    {
        LockGuard lock{mutex};
        prods.erase(index);
        incomplete.erase(index);
    }
};

const ChunkInfo ProdMap::emptyChunkInfo{};

/******************************************************************************/

/// Implementation of an iterator over chunks of data in the product store
class ProdStore::ChunkInfoIterator::Impl final
{
    ProdMap&               prods;
    ProdIndex              prodIndex;
    ChunkIndex             chunkIndex;
    static const ChunkInfo emptyChunkInfo;

public:
    /**
     * Constructs. Sets the current chunk of data to the given one.
     * @pre                  `startWith == true`
     * @param[in] prods      Collection of products
     * @param[in] startWith  Information on data-chunk with which to start
     * @see                  `ChunkInfo::operator bool()`
     */
    Impl(   ProdMap&         prods,
            const ChunkInfo& startWith)
        : prods{prods}
        , prodIndex{}
        , chunkIndex{}
    {
        if (!startWith)
            throw INVALID_ARGUMENT("Empty data-chunk information");
        prodIndex = startWith.getProdIndex();
        chunkIndex = startWith.getIndex();
        const auto earliest = prods.getEarliest();
        if (prodIndex < earliest) {
            prodIndex = earliest;
            chunkIndex = 0;
        }
    }

    /**
     * Returns information on the chunk of data that the product-store
     * contains and that is closest to but not earlier than the current
     * chunk.
     * @return  Information on the chunk or the empty chunk if such a chunk
     *          doesn't exist
     */
    const ChunkInfo operator *()
    {
        ProdInfo prodInfo;
        for (; prodIndex <= prods.getLatest(); ++prodIndex) {
            if (prods.getProdInfo(prodIndex, prodInfo)) {
                auto numChunks = prodInfo.getNumChunks();
                for (; chunkIndex < numChunks; ++chunkIndex) {
                    auto chunkInfo = prodInfo.makeChunkInfo(chunkIndex);
                    if (prods.haveChunk(chunkInfo))
                        return chunkInfo;
                }
            }
            chunkIndex = 0;
        }
        return emptyChunkInfo;
    }

    /**
     * Advances to the next chunk of data.
     * @return  This instance
     */
    void operator ++()
    {
        ++chunkIndex;
    }
};

const ChunkInfo ProdStore::ChunkInfoIterator::Impl::emptyChunkInfo{};

ProdStore::ChunkInfoIterator::ChunkInfoIterator(Impl* impl)
    : pImpl{impl}
{}

const ChunkInfo ProdStore::ChunkInfoIterator::operator *()
{
    return pImpl->operator *();
}

ProdStore::ChunkInfoIterator& ProdStore::ChunkInfoIterator::operator ++()
{
    pImpl->operator ++();
    return *this;
}

/******************************************************************************/

class ProdStore::Impl final
{
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
    ProdMap                                    prods;
    /// Concurrent-access mutex
    mutable Mutex                              mutex;
    /// Product-deletion delay-queue
    FixedDelayQueue<ProdIndex, Duration>       delayQ;
    /// Thread for deleting products whose residence-time exceeds the minimum
    std::thread                                deletionThread;
    mutable std::exception_ptr                 exception;
    ProdIndex                                  earliest;
    ProdIndex                                  latest;
    static const ChunkInfo                     emptyChunkInfo;

    void setAndThrowException() const
    {
        LockGuard lock{mutex};
        exception = std::current_exception();
        throw;
    }

    void throwIfException() const
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
    }

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
            for (;;)
                prods.erase(delayQ.pop());
    	}
    	catch (const std::exception& e) {
    	    setAndThrowException();
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
        , mutex{}
        , delayQ{Duration(static_cast<Duration::rep>(residence*1000))}
        , deletionThread{std::thread([this]{deleteOldProds();})}
        , exception{}
        , earliest{}
        , latest{}
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
            ::pthread_cancel(deletionThread.native_handle());
            deletionThread.join();
        }
        catch (const std::exception& e) {
            log_error(e);
        }
        try {
            if (file.is_open())
                persist();
        }
        catch (const std::exception& e) {
            log_error(e);
        }
    }

    /**
     * Adds an entire product. Does nothing if the product has already been
     * added. If the product is added, then it will be removed after the minimum
     * residence time has elapsed.
     * @param[in] prod   Product to be added
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    void add(Product& prod)
    {
        throwIfException();
        try {
            const bool isNew = prods.add(prod);
            if (isNew)
                delayQ.push(prod.getIndex());
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
    }

    /**
     * Adds something to a product -- creating the product if necessary. Does
     * nothing if the product already exists and already has the thing. If the
     * product is created, then it will be removed after the minimum residence
     * time has elapsed.
     * @param[in]  thing      Thing to be added (e.g., `ProdInfo`,
     *                       `LatentChunk`)
     * @param[in]  prodIndex  Index of the corresponding product
     * @param[out] prod       Corresponding product. Set iff return status
     *                        indicates that product is complete.
     * @return                Status of the addition
     * @exceptionsafety       Basic guarantee
     * @threadsafety          Safe
     */
    template<class T>
    AddStatus add(
            T               thing,
            const ProdIndex prodIndex,
            Product&        prod)
    {
        throwIfException();
        ProdEntry* entry;
        AddStatus  status{};
        try {
            status = prods.add(thing, &entry);
            if (status.isNew())
                delayQ.push(prodIndex);
            prod = entry->getProduct();
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        if (entry->isReady())
            status.setComplete();
        return status;
    }

    /**
     * Returns the number of products in the store -- both complete and
     * incomplete.
     * @return Number of products in the store
     */
    size_t size() const noexcept
    {
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
        throwIfException();
        try {
            return prods.getProdInfo(index, info);
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        return false; // To accommodate Eclipse
    }

    /**
     * Indicates if this instance contains a given chunk of data.
     * @param[in] info  Information on the chunk
     * @retval `true`   Chunk exists
     * @retval `false`  Chunk doesn't exist
     */
    bool haveChunk(const ChunkInfo& info) const
    {
        throwIfException();
        try {
            return prods.haveChunk(info);
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        return false; // To accommodate Eclipse
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
        throwIfException();
        try {
            return prods.getChunk(info, chunk);
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        return false; // To accommodate Eclipse
    }

    /**
     * Returns information on the earliest missing chunk of data.
     * @return           Information on the earliest missing data-chunk or empty
     *                   information if no such chunk exists
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     * @see `ChunkInfo::operator bool()`
     */
    ChunkInfo identifyEarliestMissingChunk() const
    {
        throwIfException();
        try {
            return prods.identifyEarliestMissingChunk();
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        return emptyChunkInfo; // To accommodate Eclipse
    }

    ProdStore::ChunkInfoIterator getChunkInfoIterator(
            const ChunkInfo& startWith)
    {
        return ProdStore::ChunkInfoIterator{
            new ProdStore::ChunkInfoIterator::Impl(prods, startWith)};
    }
};

const ChunkInfo ProdStore::Impl::emptyChunkInfo{};

ProdStore::ProdStore(
        const std::string& pathname,
        const double       residence)
    : pImpl{new Impl(pathname, residence)}
{}

void ProdStore::add(Product& prod)
{
    pImpl->add(prod);
}

ProdStore::AddStatus ProdStore::add(
        const ProdInfo& prodInfo,
        Product&        prod)
{
    return pImpl->add(prodInfo, prodInfo.getIndex(), prod);
}

ProdStore::AddStatus ProdStore::add(
        LatentChunk& chunk,
        Product&     prod)
{
    return pImpl->add(chunk, chunk.getProdIndex(), prod);
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

ProdStore::ChunkInfoIterator ProdStore::getChunkInfoIterator(
        const ChunkInfo& startWith) const
{
    return pImpl->getChunkInfoIterator(startWith);
}

} // namespace
