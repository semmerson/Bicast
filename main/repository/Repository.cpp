/**
 * Repository of data-products.
 *
 *        File: Repository.cpp
 *  Created on: Dec 23, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include "Repository.h"

#include "error.h"
#include "FileUtil.h"
#include "HashMapQueue.h"
#include "HashSetQueue.h"
#include "ProdFile.h"
#include "Watcher.h"

#include <condition_variable>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <libgen.h>
#include <limits.h>
#include <mutex>
#include <queue>
#include <sys/stat.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <unordered_map>

namespace hycast {

/**
 * @tparam PF  Product-file type
 */
struct ProdEntry
{
    ProdInfo  prodInfo;  ///< Product information
    ProdFile  prodFile;  ///< Product file
    ProdId    prev;      ///< Previous entry in queue
    ProdId    next;      ///< Next entry in queue

    ProdEntry()
        : prodFile{}
        , prodInfo{}
        , prev{}
        , next{}
    {}

    ProdEntry(ProdFile prodFile)
        : prodInfo()
        , prodFile(prodFile)
        , prev{}
        , next{}
    {}

    ProdEntry(ProdFile&& prodFile)
        : prodInfo()
        , prodFile(prodFile)
        , prev{}
        , next{}
    {}

    virtual ~ProdEntry() noexcept
    {}

    inline ProdEntry& operator=(const ProdEntry& rhs) =default;

    bool save(const ProdInfo prodInfo) {
        bool saved;
        if (this->prodInfo) {
            saved = false;
        }
        else {
            const auto prodSize = prodInfo.getSize();
            const auto knownSize = prodFile.getProdSize();
            if (prodSize != knownSize)
                throw INVALID_ARGUMENT("Known product size (" + std::to_string(knownSize) +
                        " bytes) doesn't equal given product-size (" + std::to_string(prodSize) +
                        " bytes)");
            this->prodInfo = prodInfo;
            saved = true;
        }
        return saved;
    }

    bool save(const DataSeg dataSeg) {
        return prodFile.save(dataSeg);
    }

    inline ProdSize getProdSize() const {
        return prodInfo.getSize();
    }

    /**
     * @return  Product metadata. Will test false if it hasn't been set by `set(const ProdInfo)`.
     * @see `set(const ProdInfo)`
     */
    inline const ProdInfo& getProdInfo() const
    {
        return prodInfo;
    }

    inline const std::string& getProdName() const
    {
        return prodInfo.getName();
    }

    inline DataSeg getDataSeg(const DataSegId& segId) const {
        return DataSeg{segId, prodFile.getProdSize(), prodFile.getData(segId.offset)};
    }

    inline bool exists(const ProdSize offset) const
    {
        return prodFile.exists(offset);
    }

    inline std::string to_string() const
    {
        return prodInfo.to_string();
    }

    inline bool isComplete() const noexcept {
        return prodInfo && prodFile.isComplete();
    }

    inline void rename(const int rootFd) const {
        prodFile.rename(rootFd, prodInfo.getName());
    }

    inline void open(const int rootFd) const {
        prodFile.open(rootFd);
    }

    inline void close() const {
        prodFile.close();
    }
};

/**************************************************************************************************/

/**
 * Abstract, base implementation of a repository of temporary data-products.
 */
class Repository::Impl
{
protected:
    using SetQueue = HashSetQueue<ProdId>;
    using MapQueue = HashMapQueue<ProdId, ProdEntry>;

    mutable Mutex     mutex;        ///< For concurrency
    mutable Cond      cond;         ///< For concurrency
    MapQueue          allProds;     ///< All existing products
    SetQueue          openProds;    ///< Open products
    const std::string rootPathname; ///< Absolute pathname of root directory of repository
    size_t            rootPrefixLen;///< Length in bytes of root pathname prefix
    int               rootFd;       ///< File descriptor open on root-directory of repository
    const SegSize     segSize;      ///< Size of canonical data-segment in bytes
    size_t            maxOpenFiles; ///< Max number open files

    Impl(   const std::string& rootDir,
            const SegSize      segSize,
            const size_t       maxOpenFiles)
        : mutex{}
        , cond()
        , allProds{maxOpenFiles}
        , openProds{maxOpenFiles}
        , rootPathname(makeAbsolute(rootDir))
        , rootPrefixLen{rootPathname.size() + 1}
        , rootFd(-1)
        , segSize{segSize}
        , maxOpenFiles{maxOpenFiles}
    {
        if (maxOpenFiles <= 0)
            throw INVALID_ARGUMENT("maxOpenFiles=" + std::to_string(maxOpenFiles));

        ensureDir(rootPathname, 0755); // Only owner can write

        rootFd = ::open(rootPathname.data(), O_RDONLY);
        if (rootFd == -1)
            throw SYSTEM_ERROR("Couldn't open root-directory of repository, \"" + rootPathname +
                    "\"");

        try {
            if (maxOpenFiles == 0)
                throw INVALID_ARGUMENT("Maximum number of open files is zero");
        } // `rootFd` is open
        catch (const std::exception& ex) {
            ::close(rootFd);
        }
    }

    void ensureRoom() {
        while (openProds.size() >= maxOpenFiles) {
            allProds.get(openProds.front())->close();
            openProds.pop();
        }
    }

    static bool tryHardLink(
            const std::string& extantPath,
            const std::string& linkPath)
    {
        LOG_DEBUG("Linking \"" + linkPath + "\" to \"" + extantPath + "\"");
        ensureDir(dirPath(linkPath), 0700);
        return ::link(extantPath.data(), linkPath.data()) == 0;
    }

    static void hardLink(
            const std::string& extantPath,
            const std::string& linkPath)
    {
        if (!tryHardLink(extantPath, linkPath))
            throw SYSTEM_ERROR("Couldn't link \"" + linkPath + "\" to \"" +
                    extantPath + "\"");
    }

    static bool trySoftLink(
            const std::string& extantPath,
            const std::string& linkPath)
    {
        LOG_DEBUG("Linking \"" + linkPath + "\" to \"" + extantPath + "\"");
        ensureDir(dirPath(linkPath), 0700);
        return ::symlink(extantPath.data(), linkPath.data()) == 0;
    }

    static void softLink(
            const std::string& extantPath,
            const std::string& linkPath)
    {
        if (!trySoftLink(extantPath, linkPath))
            throw SYSTEM_ERROR("Couldn't link \"" + linkPath + "\" to \"" +
                    extantPath + "\"");
    }

    /**
     * Returns the product-name corresponding to the absolute pathname of a product-file.
     *
     * @param[in] pathname  Absolute pathname of a product-file
     * @return              Corresponding product-name
     */
    std::string getProdName(const std::string& pathname) const
    {
        return pathname.substr(rootPathname.length()+1); // Remove `rootPathname+"/"`
    }

public:
    virtual ~Impl() noexcept {
        if (rootFd >= 0)
            ::close(rootFd);
    }

    SegSize getSegSize() const noexcept
    {
        return segSize;
    }

    const std::string& getRootDir() const noexcept
    {
        return rootPathname;
    }

    virtual ProdInfo getProdInfo(const ProdId prodId) =0;

    virtual DataSeg getDataSeg(const DataSegId segId) =0;

    /**
     * Returns the absolute pathname of the file corresponding to a product name.
     *
     * @pre                 `name.size() > 0`
     * @param[in] prodName  Product name
     * @return              Absolute pathname of corresponding file
     */
    std::string getPathname(const std::string& name) const
    {
        LOG_ASSERT(name.size());
        return rootPathname + "/" + name;
    }

    /**
     * Returns information on the next product to process. Blocks until one is ready.
     *
     * @return              Information on the next product to process
     * @throws SystemError  System failure
     * @threadsafety        Compatible but unsafe
     */
    virtual ProdInfo getNextProd() =0;

#if 0
    /**
     * Performs cleanup actions. Closes product-files that haven't been accessed
     * in 24 hours.
     *
     * @threadsafety       Safe
     * @exceptionsafety    Basic guarantee
     * @cancellationpoint  Yes
     */
    void cleanup()
    {
        Guard guard{mutex};

        ProdId next;
        for (auto prodId = headIndex; prodId; prodId = next) {
            auto iter = allProds.find(prodId);
            if (iter == allProds.end())
                break;
            Entry& prodEntry = iter.second;
            next = prodEntry.next;
            if (time(nullptr) - prodEntry.when <= 86400)
                break;
            allProds.erase(iter);
        }
    }
#endif
};

/******************************************************************************/

Repository::Repository() noexcept =default;

Repository::Repository(Impl* impl) noexcept
    : pImpl(impl) {
}

Repository::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

SegSize Repository::getMaxSegSize() const noexcept {
    return pImpl->getSegSize();
}

const std::string& Repository::getRootDir() const noexcept {
    return pImpl->getRootDir();
}

ProdInfo Repository::getProdInfo(const ProdId prodId) const {
    return pImpl->getProdInfo(prodId);
}

DataSeg Repository::getDataSeg(const DataSegId segId) const {
    return pImpl->getDataSeg(segId);
}

/******************************************************************************/
/******************************************************************************/

/**
 * Implementation of a publisher's repository.
 */
class PubRepo::Impl final : public Repository::Impl
{
    Watcher            watcher;     ///< Watches filename hierarchy
    std::queue<ProdId> prodQueue;   ///< Queue of products to be sent
    ProdId             prodId;      ///< Next product-identifier

    /**
     * Adds a new product to the set of all products and to the queue of products to be sent.
     *
     * @pre                  State is unlocked
     * @param[in] prodId     Product-identifier
     * @param[in] prodFile   Product-file
     */
    void addProd(
            const ProdId prodId,
            ProdFile     prodFile) {
        Guard guard(mutex);

        allProds.push(prodId, ProdEntry{prodFile});
        prodQueue.push(prodId);
        cond.notify_all();
    }

    /**
     * Returns the product-entry corresponding to a product-ID. The corresponding product is open
     * and at the back of the open-products set.
     *
     * @pre               State is locked
     * @param[in] prodId  Product identifier
     * @param[in] open    Does the product need to be open?
     * @return            Corresponding product-entry. The product is open.
     * @retval `nullptr`  No such entry exists
     * @post              State is locked
     * @exceptionsafety   Basic guarantee
     */
    ProdEntry* getProdEntry(const ProdId prodId) {
        LOG_ASSERT(!mutex.try_lock());

        auto prodEntry = allProds.get(prodId);

        if (prodEntry) {
            if (!openProds.erase(prodId)) {
                // Product wasn't open
                ensureRoom();
                prodEntry->open(rootFd);
            }
            openProds.push(prodId);
        }

        return prodEntry;
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize,
#ifdef OPEN_MAX
            const long         maxOpenFiles = OPEN_MAX/2)
#else
            const long         maxOpenFiles = sysconf(_SC_OPEN_MAX)/2)
#endif
        : Repository::Impl{rootPathname, segSize, static_cast<size_t>(maxOpenFiles)}
        , watcher(this->rootPathname) // Is absolute pathname
        , prodQueue()
        , prodId()
    {
        if (maxOpenFiles <= 0)
            throw INVALID_ARGUMENT("maxOpenFiles=" + std::to_string(maxOpenFiles));
    }

    /**
     * Links to a file (which could be a directory) that's outside the repository. The watcher will
     * notice and process the link.
     *
     * @param[in] pathname       Absolute pathname (with no trailing '/') of the file or directory
     *                           to be linked to
     * @param[in] prodName       Product name if the pathname references a file and Product name
     *                           prefix if the pathname references a directory
     * @throws InvalidArgument  `pathname` is empty or a relative pathname
     * @throws InvalidArgument  `prodName` is invalid
     */
    void link(
            const std::string& pathname,
            const std::string& prodName)
    {
        if (pathname.size() == 0 || pathname[0] != '/')
            throw INVALID_ARGUMENT("Invalid pathname: \"" + pathname + "\"");
        if (prodName.size() == 0 || prodName[0] == '/')
            throw INVALID_ARGUMENT("Invalid product name: \"" + prodName + "\"");

        const std::string extantPath = makeAbsolute(pathname);
        const std::string linkPath = getPathname(prodName);
        Guard             guard(mutex);

        ensureDir(dirPath(linkPath), 0700);

        if (!tryHardLink(extantPath, linkPath) &&
                !trySoftLink(extantPath, linkPath))
            throw SYSTEM_ERROR("Couldn't link file \"" + linkPath + "\" to \"" + extantPath + "\"");
    }

    /**
     * Returns information on the next product to publish. Blocks until one is
     * ready. Watches the repository's directory hierarchy for new files. A
     * product-entry is created and added to the set of active product-entries.
     * for each new non-directory file in the repository's hierarchy:
     *
     * @return              Information on the next product to publish
     * @throws SystemError  System failure
     * @threadsafety        Compatible but unsafe
     */
    ProdInfo getNextProd()
    {
        ProdInfo prodInfo{};
        String   prodName;

        try {
            Watcher::WatchEvent event;
            watcher.getEvent(event);

            //LOG_DEBUG("event.pathname=%s", event.pathname.data());
            //LOG_DEBUG("rootPrefixLen=%zu", rootPrefixLen);
            prodName = event.pathname.substr(rootPrefixLen);
            ProdFile prodFile(rootFd, prodName, segSize);

            const ProdId prodId(prodName);
            prodInfo = ProdInfo(prodId, prodName, prodFile.getProdSize());

            addProd(prodId, prodFile);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create product-file for \"" + prodName +
                    "\""));
        }

        return prodInfo;
    }

    /**
     * Returns the product-information corresponding to a product-index.
     *
     * @pre                  Instance is unlocked
     * @param[in] prodId     Product identifier
     * @return               Corresponding product-information. Will test false if no such
     *                       information exists.
     */
    ProdInfo getProdInfo(const ProdId prodId)
    {
        Guard      guard{mutex};
        const auto prodEntry = allProds.get(prodId);

        if (prodEntry == nullptr) {
            static const ProdInfo prodInfo{};
            return prodInfo;
        }

        return prodEntry->getProdInfo();
    }

    /**
     * Returns the in-memory data-segment corresponding to a segment identifier.
     *
     * @pre              State is unlocked
     * @param[in] segId  Data-segment identifier
     * @return           Corresponding in-memory data-segment Will test false if no such segment
     *                   exists.
     * @post             State is unlocked
     * @see `DataSeg::operator bool()`
     */
    DataSeg getDataSeg(const DataSegId segId)
    {
        Guard      guard{mutex};
        const auto prodEntry = getProdEntry(segId.prodId);

        if (!prodEntry) {
            static const DataSeg dataSeg{};
            return dataSeg;
        }

        return prodEntry->getDataSeg(segId);
    }
};

/******************************************************************************/

PubRepo::PubRepo()
    : Repository{nullptr}
{}

PubRepo::PubRepo(
        const std::string& rootPathname,
        const SegSize      segSize,
        const long         maxOpenFiles)
    : Repository(new Impl(rootPathname, segSize, maxOpenFiles)) {
}

void PubRepo::link(
        const std::string& pathname,
        const std::string& prodName) {
    return static_cast<Impl*>(pImpl.get())->link(pathname, prodName);
}

ProdInfo PubRepo::getNextProd() const {
    return static_cast<Impl*>(pImpl.get())->getNextProd();
}

/******************************************************************************/
/******************************************************************************/

/**
 * Implementation of a subscriber's repository.
 */
class SubRepo::Impl final : public Repository::Impl
{
    std::queue<ProdInfo>             prodQueue; ///< Queue of completed products

    void makeRoom()
    {
        while (openProds.size() >= maxOpenFiles) {
            allProds.get(openProds.front())->close();
            openProds.pop();
        }
    }

    /**
     * Returns the product-entry corresponding to a product-ID. The product-entry is created if it
     * doesn't exist. The corresponding product is open and at the back of the open-products set.
     *
     * @pre                 State is locked
     * @param[in] prodId    Product identifier
     * @param[in] prodSize  Size of product in bytes
     * @return              Corresponding product-entry. The product is open.
     * @post                State is locked
     * @exceptionsafety     Basic guarantee
     */
    ProdEntry& getProdEntry(
            const ProdId   prodId,
            const ProdSize prodSize)
    {
        LOG_ASSERT(!mutex.try_lock());

        auto prodEntry = allProds.get(prodId);

        if (prodEntry == nullptr) {
            auto      prodFile = ProdFile(rootFd, ".incomplete/" + prodId.to_string(), segSize,
                    prodSize);
            ProdEntry entry{prodFile};
            prodEntry = allProds.push(prodId, entry);
        }

        if (!openProds.erase(prodId)) {
            ensureRoom();
            prodEntry->open(rootFd); // Idempotent
        }
        openProds.push(prodId);

        return *prodEntry;
    }

    /**
     * Returns the product-entry corresponding to a product-ID. The corresponding product is open
     * and at the back of the open-products set.
     *
     * @pre               State is locked
     * @param[in] prodId  Product identifier
     * @param[in] open    Does the product need to be open?
     * @return            Corresponding product-entry. The product is open.
     * @retval `nullptr`  No such entry exists
     * @post              State is locked
     * @exceptionsafety   Basic guarantee
     */
    ProdEntry* getProdEntry(const ProdId prodId)
    {
        LOG_ASSERT(!mutex.try_lock());

        auto prodEntry = allProds.get(prodId);

        if (prodEntry) {
            if (!openProds.erase(prodId)) {
                // Product wasn't open
                ensureRoom();
                prodEntry->open(rootFd);
            }
            openProds.push(prodId);
        }

        return prodEntry;
    }

    /**
     * Finishes processing a data-product. The product's metadata is added to the outgoing
     * -product queue if the product is complete.
     *
     * @pre                 State is locked
     * @param[in] prodFile  Product entry
     * @post                State is locked
     */
    void finishIfComplete(const ProdEntry& prodEntry) {
        if (prodEntry.isComplete()) {
            prodEntry.rename(rootFd);
            prodQueue.push(prodEntry.getProdInfo());
            cond.notify_all();
        }
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize,
            const size_t       maxOpenFiles)
        : Repository::Impl{rootPathname, segSize, maxOpenFiles}
    {}

    /**
     * Saves product-information. Creates a new product if necessary. If the resulting product
     * becomes complete, then it will be added to the outgoing queue.
     *
     * @param[in] prodInfo     Product information
     * @retval    `true``      Product information was saved
     * @retval    `false`      Product information was previously saved
     * @throw InvalidArgument  Known product has a different size
     * @throw LogicError       Metadata was previously saved
     * @throw SystemError      System failure
     * @see `getNextProd()`
     */
    bool save(const ProdInfo prodInfo) {
        Guard      guard{mutex};
        const auto prodId = prodInfo.getId();
        auto&      prodEntry = getProdEntry(prodId, prodInfo.getSize());
        auto       saved = prodEntry.save(prodInfo);

        finishIfComplete(prodEntry);
        return saved;
    }

    /**
     * Saves a data-segment in the corresponding product-file. If the resulting product becomes
     * complete, then it will be eventually returned by `getNextProd()`.
     *
     * @param[in] dataSeg      Data segment
     * @retval    `true`       This item was saved
     * @retval    `false`      This item was not saved because it already exists
     * @throw InvalidArgument  Known product has a different size
     * @throw SystemError      System failure
     * @see `getNextProd()`
     */
    bool save(const DataSeg dataSeg)
    {
        Guard      guard{mutex};
        const auto prodId = dataSeg.getId().prodId;
        auto&      prodEntry = getProdEntry(prodId, dataSeg.getProdSize());
        const bool saved = prodEntry.save(dataSeg);

        finishIfComplete(prodEntry);
        return saved;
    }

    /**
     * Returns information on the next, completed data-product. Blocks until one is available.
     *
     * @return Next, completed data-product
     */
    ProdInfo getNextProd()
    {
        Lock lock(mutex);

        cond.wait(lock, [&]{return !prodQueue.empty();});

        auto prodInfo = prodQueue.front();
        prodQueue.pop();

        return prodInfo;
    }

    /**
     * Returns information on a product if it exists.
     *
     * @param prodId     Product identifier
     * @return           Product information. Will test false if no such product exists.
     */
    ProdInfo getProdInfo(const ProdId prodId)
    {
        Guard guard{mutex};
        auto  prodEntry = allProds.get(prodId);

        if (prodEntry == nullptr) {
            static const ProdInfo prodInfo{};
            return prodInfo;
        }

        return prodEntry->getProdInfo();
    }

    /**
     * Returns the data-segment corresponding to a segment identifier if it exists.
     *
     * @param[in] segId  Segment identifier
     * @return           Corresponding memory data-segment. Will test false if
     *                   no such segment exists.
     */
    DataSeg getDataSeg(const DataSegId segId)
    {
        auto  offset = segId.offset;
        Guard guard{mutex};
        auto  prodEntry = getProdEntry(segId.prodId);

        if (prodEntry == nullptr) {
            static const DataSeg dataSeg{};
            return dataSeg;
        }

        return  prodEntry->getDataSeg(segId);
    }

    /**
     * Indicates if complete information on a data-product exists.
     *
     * @param[in] prodId     Product identifier
     * @retval    `true`     Product information does exist
     * @retval    `false`    Product information does not exist
     */
    bool exists(const ProdId prodId)
    {
        Guard guard{mutex};
        auto  prodEntry = allProds.get(prodId);
        return prodEntry && prodEntry->prodInfo;
    }

    /**
     * Indicates if a data-segment exists.
     *
     * @param[in] segId      Data-segment identifier
     * @retval    `true`     Data-segment does exist
     * @retval    `false`    Data-segment does not exist
     */
    bool exists(const DataSegId segId)
    {
        Guard guard{mutex};
        auto  prodEntry = allProds.get(segId.prodId);
        return prodEntry != nullptr && prodEntry->prodFile.exists(segId.offset);
    }
};

/******************************************************************************/

SubRepo::SubRepo()
    : Repository{nullptr}
{}

SubRepo::SubRepo(
        const std::string& rootPathname,
        const SegSize      segSize,
        const size_t       maxOpenFiles)
    : Repository{new Impl(rootPathname, segSize, maxOpenFiles)} {
}

bool SubRepo::save(const ProdInfo prodInfo) const {
    return static_cast<Impl*>(pImpl.get())->save(prodInfo);
}

bool SubRepo::save(const DataSeg dataSeg) const {
    return static_cast<Impl*>(pImpl.get())->save(dataSeg);
}

ProdInfo SubRepo::getNextProd() const {
    return static_cast<Impl*>(pImpl.get())->getNextProd();
}

bool SubRepo::exists(const ProdId prodId) const {
    return static_cast<Impl*>(pImpl.get())->exists(prodId);
}

bool SubRepo::exists(const DataSegId segId) const {
    return static_cast<Impl*>(pImpl.get())->exists(segId);
}

} // namespace
