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
#include "LinkedHashMap.h"
#include "ProdInfo.h"
#include "Product.h"

#include <cstring>
#include <fcntl.h>
#include <list>
#include <mutex>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
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

    virtual ActualChunk getChunk(const ChunkIndex index) =0;

    virtual ChunkId identifyEarliestMissingChunk() const =0;

    virtual bool set(const ProdInfo& info) =0;

    virtual bool add(const ActualChunk& chunk) =0;

    virtual bool add(LatentChunk& chunk) =0;

    virtual bool isComplete() const noexcept =0;

    virtual const char* getData() =0;
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

ChunkId Product::identifyEarliestMissingChunk() const
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

const char* Product::getData()
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

ActualChunk Product::getChunk(const ChunkIndex index)
{
    return pImpl->getChunk(index);
}

/******************************************************************************/

class CompleteProduct::Impl : public Product::Impl
{
protected:
    Impl()
        : Product::Impl()
    {}

    Impl(const ProdInfo& prodInfo)
        : Product::Impl(prodInfo)
    {}

public:
    virtual ~Impl()
    {}

    inline ChunkId identifyEarliestMissingChunk() const
    {
        throw LOGIC_ERROR(
                "Complete product doesn't have earliest missing chunk");
    }

    inline bool haveChunk(const ChunkOffset offset) const
    {
        return offset < prodInfo.getSize();
    }

    inline bool haveChunk(const ChunkIndex index) const
    {
        return haveChunk(prodInfo.getChunkOffset(index));
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
    inline bool set(const ProdInfo& info)
    {
        throw LOGIC_ERROR(
                "Can't set product-information of complete product");
    }

    inline bool add(const ActualChunk& chunk)
    {
        throw LOGIC_ERROR("Can't add data to complete product");
    }

    inline bool add(LatentChunk& chunk)
    {
        throw LOGIC_ERROR("Can't add data to complete product");
    }

    /**
     * Indicates if this instance is complete (i.e., contains all
     * chunks-of-data).
     * @return `true` iff this instance is complete
     */
    inline bool isComplete() const noexcept
    {
        return true;
    }

    virtual ActualChunk getChunk(const ChunkIndex index) =0;

    virtual const char* getData() =0;
};

CompleteProduct::CompleteProduct()
    : Product{}
{}

CompleteProduct::CompleteProduct(Impl* ptr)
    : Product{ptr}
{}

CompleteProduct::~CompleteProduct()
{}

/******************************************************************************/

class MemoryProduct::Impl : public CompleteProduct::Impl
{
    const char* data;

public:
    Impl(   const ProdIndex    index,
            const std::string& name,
            const ProdSize     size,
            const char*        data,
            const ChunkSize    chunkSize)
        : CompleteProduct::Impl{ProdInfo{index, name, size, chunkSize}}
        , data{data}
    {}

    ActualChunk getChunk(const ChunkIndex index)
    {
        auto offset = prodInfo.getChunkOffset(index);
        return ActualChunk{prodInfo, index, data + offset};
    }

    const char* getData()
    {
        return data;
    }
};


MemoryProduct::MemoryProduct()
    : CompleteProduct{}
{}

MemoryProduct::MemoryProduct(
        const ProdIndex index,
        const ProdName& name,
        const ProdSize  size,
        const char*     data,
        const ChunkSize chunkSize)
    : CompleteProduct{new Impl{index, name, size, data, chunkSize}}
{}

/******************************************************************************/

class FileProduct::Impl : public CompleteProduct::Impl
{
    typedef std::mutex Mutex;

    /// Pathname of file
    std::string                              pathname;
    /// Product data
    void*                                    data;
    /// Collection of active instances
    static LinkedHashMap<std::string, Impl*> activeImpls;
    /// Mutex for concurrent access to collection of active instances
    static Mutex                             activeImplsMutex;

    /**
     * Opens the file.
     * @return              File descriptor of open product-file
     * @throw SystemError   Couldn't open file
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Compatible but not safe
     */
    int openFile() const
    {
        int fd = ::open(pathname.c_str(), O_RDONLY);
        if (fd == -1)
            throw SYSTEM_ERROR("Couldn't open file " + pathname, errno);
        return fd;
    }

    /**
     * Sets the product information.
     * @pre                    `pathname` and `fd` are valid
     * @param[in] fd           File descriptor of product-file
     * @param[in] prodIndex    Product index
     * @param[in] chunkSize    Size of a canonical data-chunk
     * @throw InvalidArgument  File is too large
     * @throw SystemError      Couldn't get file metadata
     * @exceptionsafety        Strong guarantee
     * @threadsafety           Compatible but not safe
     */
    void setProdInfo(
            const int       fd,
            const ProdIndex prodIndex,
            const ChunkSize chunkSize)
    {
        struct stat statBuf;
        if (::fstat(fd, &statBuf))
            throw SYSTEM_ERROR("Couldn't get information on file " +
                    pathname, errno);
        if (statBuf.st_size > ProdSize::prodSizeMax)
            throw INVALID_ARGUMENT("File " + pathname + " has " +
                    std::to_string(statBuf.st_size) + " bytes; Maximum "
                    "allowable is " +
                    std::to_string(ProdSize::prodSizeMax));
        ProdSize prodSize{static_cast<ProdSize::type>(statBuf.st_size)};
        prodInfo = ProdInfo{prodIndex, pathname, prodSize, chunkSize};
    }

    /**
     * Memory-maps the file.
     * @param[in] fd       File descriptor of open file
     * @retval 0           Success
     * @return             System error number
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Compatible but not safe
     */
    int memoryMap(const int fd)
    {
        auto ptr = ::mmap(0, prodInfo.getSize(), PROT_READ, MAP_PRIVATE, fd, 0);
        if (ptr == MAP_FAILED)
            return errno;
        data = ptr;
        return 0;
    }

    /**
     * Un-memory-maps the file.
     * @pre                `activeImplsMutex` is locked
     * @pre                `prodInfo` is valid
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Compatible but not safe
     */
    void unMemoryMap()
    {
        ::munmap(data, prodInfo.getSize());
        data = nullptr;
    }

    /**
     * Makes this instance the most-recently accessed one.
     * @pre                  `data != nullptr`
     * @exceptionsafety      Strong guarantee for this instance; Basic guarantee
     *                       for `activeImpls`.
     * @threadsafety         Safe
     */
    void makeMostRecentlyAccessed()
    {
        std::lock_guard<Mutex> lock{activeImplsMutex};
        activeImpls.remove(pathname);
        activeImpls.insert(pathname, this);
    }

    /**
     * Activates this instance. Memory-maps the file and adds this instance
     * to the collection of active instances.
     * @pre                  `prodInfo` is valid
     * @pre                  `data == nullptr`
     * @throw RuntimeError   Couldn't free-up sufficient resources
     * @throw SystemError    Couldn't open file
     * @throw SystemError    Couldn't memory-map file
     * @exceptionsafety      Strong guarantee for this instance; Basic guarantee
     *                       for `activeImpls`.
     * @threadsafety         Compatible but not safe
     */
    void activate()
    {
        std::lock_guard<Mutex> lock{activeImplsMutex};
        for (;;) {
            auto fd = openFile();
            try {
                auto status = memoryMap(fd);
                if (status == 0) {
                    try {
                        activeImpls.insert(pathname, this);
                    }
                    catch (const std::exception& ex) {
                        unMemoryMap();
                        std::throw_with_nested(RUNTIME_ERROR(
                                "Couldn't add FileProduct " + pathname +
                                " to collection of active instances"));
                    }
                    ::close(fd);
                    break;
                }
                if (status != EMFILE)
                    throw SYSTEM_ERROR("Couldn't memory-map file " + pathname,
                            status);
                Impl* implPtr;
                if (!activeImpls.pop(implPtr))
                    throw RUNTIME_ERROR("Couldn't free sufficient resources: "
                            "All FileProduct-s are inactive");
                implPtr->unMemoryMap();
            }
            catch (const std::exception& ex) {
                ::close(fd);
                throw;
            }
        }
    }

    /**
     * Removes this instance from the list of active instances and
     * un-memory-maps the file.
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    void deactivate()
    {
        std::lock_guard<Mutex> lock{activeImplsMutex};
        activeImpls.remove(pathname);
        unMemoryMap();
    }

    /**
     * Ensures that this instance is active.
     * @pre              `prodInfo` is valid
     * @exceptionsafety  Strong guarantee for this instance; Basic guarantee for
     *                   `activeImpls`.
     * @threadsafety     Compatible but not safe
     */
    inline void ensureActive()
    {
        data
            ? makeMostRecentlyAccessed()
            : activate();
    }

public:
    /**
     * Constructs.
     * @param[in] prodIndex    Product index
     * @param[in] pathname     File pathname
     * @param[in] chunkSize    Size of canonical data-chunk
     * @throw SystemError      Couldn't open file
     * @throw InvalidArgument  File is too large
     * @throw SystemError      Couldn't get file metadata
     * @throw SystemError      Couldn't memory-map file
     */
    Impl(   const ProdIndex    prodIndex,
            const std::string& pathname,
            const ChunkSize    chunkSize)
        : CompleteProduct::Impl() // Eclipse misunderstands `{}`
        , pathname{pathname}
        , data{nullptr}
    {
        int fd = openFile();
        try {
            setProdInfo(fd, prodIndex, chunkSize);
            ::close(fd);
        }
        catch (const std::exception& ex) {
            ::close(fd);
            throw;
        }
        activate();
    }

    /**
     * Destroys.
     */
    ~Impl() noexcept
    {
        if (data)
            deactivate();
    }

    /**
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Compatible but not safe
     */
    ActualChunk getChunk(const ChunkIndex index)
    {
        auto offset = prodInfo.getChunkOffset(index);
        ensureActive();
        return ActualChunk{prodInfo, index, static_cast<char*>(data) + offset};
    }

    /**
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Compatible but not safe
     */
    const char* getData()
    {
        ensureActive();
        return static_cast<char*>(data);
    }
};

LinkedHashMap<std::string, FileProduct::Impl*> FileProduct::Impl::activeImpls;
FileProduct::Impl::Mutex                       FileProduct::Impl::activeImplsMutex;

FileProduct::FileProduct()
    : CompleteProduct{}
{}

FileProduct::FileProduct(
        const ProdIndex    prodIndex,
        const std::string& pathname,
        const ChunkSize    chunkSize)
    : CompleteProduct{new Impl{prodIndex, pathname, chunkSize}}
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

    ActualChunk getChunk(const ChunkIndex index)
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
    const char* getData()
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
