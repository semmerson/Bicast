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
#include <map>
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
        : prod{PartialProduct{prodInfo}}
    {}

    ProdEntry(ProdInfo&& prodInfo)
        : prod{PartialProduct{prodInfo}}
    {}

    ProdEntry(LatentChunk& chunk)
        : ProdEntry{ProdInfo{chunk.getProdIndex(), chunk.getProdSize(),
                chunk.getCanonSize()}}
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

    inline ChunkId identifyEarliestMissingChunk() const
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

    inline bool isComplete() const
    {
        return prod.isComplete();
    }

    const inline Product& getProduct() const
    {
        return prod;
    }

    inline bool haveChunk(const ChunkIndex index) const
    {
        return prod.haveChunk(index);
    }

    inline ActualChunk getChunk(const ChunkIndex index)
    {
        return prod.getChunk(index);
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
    static const ChunkId                     emptyChunkId;

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
            if (index.isEarlierThan(earliest))
                earliest = index;
            if (latest.isEarlierThan(index))
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
        auto       pair = prods.emplace(prodIndex, ProdEntry{prod});
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
    RecvStatus add(
            const ProdInfo&   prodInfo,
            ProdEntry** const prodEntry)
    {
        LockGuard   lock{mutex};
        ProdEntry*  entry;
        RecvStatus  status{};
        const auto  prodIndex = prodInfo.getIndex();
        auto        iter = prods.find(prodIndex);
        if (iter == prods.end()) {
            status.setNew();
            entry = &prods.emplace(prodIndex,
                    ProdEntry{prodInfo}).first->second;
            incomplete[prodIndex] = entry;
            updateEndIndexes(prodIndex);
        }
        else {
            entry = &iter->second;
            if (entry->set(prodInfo))
                status.setNew();
            if (entry->isComplete()) {
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
    RecvStatus add(
            LatentChunk&      chunk,
            ProdEntry** const prodEntry)
    {
        LockGuard  lock{mutex};
        ProdEntry* entry;
        RecvStatus status{};
        const auto prodIndex = chunk.getProdIndex();
        auto       iter = prods.find(prodIndex);
        if (iter == prods.end()) {
            status.setNew();
            entry = &prods.emplace(prodIndex, ProdEntry{chunk}).first->second;
            incomplete[prodIndex] = entry;
            updateEndIndexes(prodIndex);
        }
        else {
            entry = &iter->second;
            if (entry->add(chunk))
                status.setNew();
            if (entry->isComplete()) {
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
     * @return            Information on the given product. Will be invalid
     *                    if no such product is found.
     * @see `ProdInfo::operator bool()`
     */
    ProdInfo getProdInfo(
            const ProdIndex index) const
    {
        LockGuard lock{mutex};
        auto iter = prods.find(index);
        return (iter == prods.end())
            ? ProdInfo{}
            : iter->second.getInfo();
    }

    /**
     * Indicates if this instance contains a given chunk of data.
     * @param[in] id    Chunk Id
     * @retval `true`   Chunk exists
     * @retval `false`  Chunk doesn't exist
     */
    bool haveChunk(const ChunkId& id) const
    {
        LockGuard lock{mutex};
        auto      iter = prods.find(id.getProdIndex());
        if (iter == prods.end())
            return false;
        return iter->second.haveChunk(id.getChunkIndex());
    }

    /**
     * Returns the chunk of data corresponding to a chunk-ID.
     * @param[in]  id     Chunk ID
     * @return            Data-chunk. Will be invalid if no such chunk exists.
     * @see `Chunk::operator bool()`
     */
    ActualChunk getChunk(const ChunkId& id)
    {
        LockGuard lock{mutex};
        auto      iter = prods.find(id.getProdIndex());
        return (iter == prods.end())
                ? ActualChunk{}
                : iter->second.getChunk(id.getChunkIndex());
    }

    ChunkId identifyEarliestMissingChunk() const
    {
        LockGuard lock{mutex};
        auto iter = incomplete.begin();
        return (iter == incomplete.end())
                ? emptyChunkId
                : iter->second->identifyEarliestMissingChunk();
    }

    void erase(const ProdIndex& index)
    {
        LockGuard lock{mutex};
        prods.erase(index);
        auto iter = incomplete.find(index);
        if (iter != incomplete.end()) {
            LOG_WARN("Deleting incomplete product " +
                    iter->second->getInfo().to_string());
            incomplete.erase(iter);
        }
    }
};

const ChunkId ProdMap::emptyChunkId{};

/******************************************************************************/

/// Implementation of an iterator over chunks of data in the product store
class ProdStore::ChunkInfoIterator::Impl final
{
    ProdMap&               prods;
    ProdIndex              prodIndex;
    ChunkIndex             chunkIndex;
    static const ChunkId invalidChunkId;

public:
    /**
     * Constructs. Sets the current chunk of data to the given one.
     * @pre                  `startWith == true`
     * @param[in] prods      Collection of products
     * @param[in] startWith  Information on data-chunk with which to start
     * @see                  `ChunkInfo::operator bool()`
     */
    Impl(   ProdMap&       prods,
            const ChunkId& startWith)
        // g++ 4.8 doesn't support `{}` reference-initialization; clang does
        : prods(prods)
        , prodIndex{}
        , chunkIndex{}
    {
        if (!startWith)
            throw INVALID_ARGUMENT("Empty data-chunk information");
        prodIndex = startWith.getProdIndex();
        const auto prodInfo = prods.getProdInfo(prodIndex);
        const auto earliest = prods.getEarliest();
        if (!prodInfo) {
            // `startWith` product not found
            prodIndex = earliest;
            chunkIndex = 0;
        }
        else {
            // `startWith` product found
            if (!prodIndex.isEarlierThan(earliest)) {
                chunkIndex = prodInfo.getChunkIndex(startWith);
            }
            else {
                prodIndex = earliest;
                chunkIndex = 0;
            }
        }
    }

    /**
     * Identifies the chunk of data that the product-store contains and that is
     * closest to but not earlier than the current chunk.
     * @return  Chunk identifier. Will be invalid if such a chunk doesn't exist.
     * @see `ChunkId::operator bool()`
     */
    const ChunkId operator *()
    {
        for (; prodIndex <= prods.getLatest(); ++prodIndex) {
            auto prodInfo = prods.getProdInfo(prodIndex);
            if (prodInfo) {
                auto numChunks = prodInfo.getNumChunks();
                for (; chunkIndex < numChunks; ++chunkIndex) {
                    auto chunkId = prodInfo.makeChunkId(chunkIndex);
                    if (prods.haveChunk(chunkId))
                        return chunkId;
                }
            }
            chunkIndex = 0;
        }
        return invalidChunkId;
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

const ChunkId ProdStore::ChunkInfoIterator::Impl::invalidChunkId{};

ProdStore::ChunkInfoIterator::ChunkInfoIterator(Impl* impl)
    : pImpl{impl}
{}

const ChunkId ProdStore::ChunkInfoIterator::operator *()
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
    static const ChunkId                     emptyChunkId;

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
    RecvStatus add(
            T               thing,
            const ProdIndex prodIndex,
            Product&        prod)
    {
        throwIfException();
        ProdEntry*  entry;
        RecvStatus  status{};
        try {
            status = prods.add(thing, &entry);
            if (status.isNew())
                delayQ.push(prodIndex);
            prod = entry->getProduct();
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        if (entry->isComplete())
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

    bool haveProdInfo(const ProdIndex index) const
    {
        throwIfException();
        try {
            return prods.getProdInfo(index).isComplete();
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        return false; // To accommodate Eclipse
    }

    /**
     * Returns product-information on a given data-product.
     * @param[in]  index  Index of the data-product
     * @return            Product information. Will be invalid if no such
     *                    data-product exists.
     * @see `ProdInfo::operator bool()`
     */
    ProdInfo getProdInfo(const ProdIndex index) const
    {
        throwIfException();
        try {
            return prods.getProdInfo(index);
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        return ProdInfo{}; // To accommodate Eclipse
    }

    /**
     * Indicates if this instance contains a given chunk of data.
     * @param[in] info  Information on the chunk
     * @retval `true`   Chunk exists
     * @retval `false`  Chunk doesn't exist
     */
    bool haveChunk(const ChunkId& info) const
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

    ActualChunk getChunk(const ChunkId& id)
    {
        throwIfException();
        try {
            return prods.getChunk(id);
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        return ActualChunk{}; // To accommodate Eclipse
    }

    /**
     * Returns information on the earliest missing chunk of data.
     * @return           Information on the earliest missing data-chunk or empty
     *                   information if no such chunk exists
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     * @see `ChunkInfo::operator bool()`
     */
    ChunkId identifyEarliestMissingChunk() const
    {
        throwIfException();
        try {
            return prods.identifyEarliestMissingChunk();
        }
        catch (const std::exception& ex) {
            setAndThrowException();
        }
        return emptyChunkId; // To accommodate Eclipse
    }

    ProdStore::ChunkInfoIterator getChunkInfoIterator(
            const ChunkId& startWith)
    {
        return ProdStore::ChunkInfoIterator{
            new ProdStore::ChunkInfoIterator::Impl(prods, startWith)};
    }
};

const ChunkId ProdStore::Impl::emptyChunkId{};

ProdStore::ProdStore(
        const std::string& pathname,
        const double       residence)
    : pImpl{new Impl(pathname, residence)}
{}

void ProdStore::add(Product& prod)
{
    pImpl->add(prod);
}

RecvStatus ProdStore::add(
        const ProdInfo& prodInfo,
        Product&        prod)
{
    return pImpl->add(prodInfo, prodInfo.getIndex(), prod);
}

RecvStatus ProdStore::add(
        LatentChunk& chunk,
        Product&     prod)
{
    return pImpl->add(chunk, chunk.getProdIndex(), prod);
}

size_t ProdStore::size() const noexcept
{
    return pImpl->size();
}

bool ProdStore::haveProdInfo(const ProdIndex index) const
{
    return pImpl->haveProdInfo(index);
}

ProdInfo ProdStore::getProdInfo(const ProdIndex index) const
{
    return pImpl->getProdInfo(index);
}

bool ProdStore::haveChunk(const ChunkId& id) const
{
    return pImpl->haveChunk(id);
}

ActualChunk ProdStore::getChunk(const ChunkId& id) const
{
    return pImpl->getChunk(id);
}

ChunkId ProdStore::getOldestMissingChunk() const
{
    return pImpl->identifyEarliestMissingChunk();
}

ProdStore::ChunkInfoIterator ProdStore::getChunkInfoIterator(
        const ChunkId& startWith) const
{
    return pImpl->getChunkInfoIterator(startWith);
}

} // namespace
