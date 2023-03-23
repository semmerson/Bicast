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

#include "FileUtil.h"
#include "HashSetQueue.h"
#include "Shield.h"
#include "ThreadException.h"
#include "Watcher.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <libgen.h>
#include <limits.h>
#include <map>
#include <mutex>
#include <queue>
#include <sys/stat.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace hycast {

using namespace std::chrono;

/**
 * Abstract, base implementation of a repository of transient data-products.
 */
class Repository::Impl
{
    Thread deleteThread;   ///< Thread for deleting old products

    /**
     * Adds a product to the database from an existing product-file. This function will be called
     * within a publisher and a subscriber during the initial scan of the repository.
     *
     * @param[in] absPathname  Absolute pathname of existing file
     * @retval    true         Referenced product was added
     * @retval    false        Referenced product was not added because it's a duplicate. This
     *                         shouldn't happen.
     */
    inline bool addExisting(const String& absPathname) {
        LOG_ASSERT(FileUtil::isAbsolute(absPathname));
        Guard      guard{mutex}; // Caller didn't changed state. This function will.
        const auto wasAdded = addExistingLocked(absPathname).second;
        if (wasAdded)
            cond.notify_all();
        return wasAdded;
    }

    /**
     * Deletes products that are at least as old as the keep-time.
     * @throws SystemError  Couldn't delete file
     * @throws SystemError  Couldn't delete empty directory
     */
    void deleteProds() {
        LOG_DEBUG("Deleting too-old products");
        try {
            /**
             * The following code deletes products on-time regardless of their arrival rate.
             */
            static const auto endOfTime = SysTimePoint::max();
            Lock              lock{mutex};

            auto deleteTime = SysTimePoint{deleteQueue.empty()
                    ? endOfTime
                    : deleteQueue.top().deleteTime};

            for (; ; deleteTime = deleteQueue.empty() ? endOfTime : deleteQueue.top().deleteTime) {
                if (cond.wait_until(lock, deleteTime, [&] {return (!deleteQueue.empty() &&
                        deleteQueue.top().deleteTime <= SysClock::now()) || stop;})) {
                    if (stop)
                        break;

                    // Temporarily disable thread cancellation to protect the following state change
                    Shield     shield{};
                    const auto prodId = deleteQueue.top().prodId;
                    auto       prodFile = prodEntries.at(prodId).prodFile;

                    prodFile.close(); // Idempotent
                    FileUtil::removeFileAndPrune(absPathRoot, prodFile.getPathname());
                    openProds.erase(prodId);
                    prodEntries.erase(prodId);
                    deleteQueue.pop();
                }
            }
        }
        catch (const std::exception& ex) {
            setThreadEx(ex);
        }
    }

protected:
    /// An entry in a delete-queue
    struct DeleteEntry
    {
        SysTimePoint deleteTime; ///< When the product should be deleted
        ProdId       prodId;     ///< The product's ID

        /**
         * Constructs.
         * @param[in] deleteTime  When the product should be deleted
         * @param[in] prodId      The product's ID
         */
        DeleteEntry(
                const SysTimePoint& deleteTime,
                const ProdId        prodId)
            : deleteTime(deleteTime)
            , prodId(prodId)
        {}

        /**
         * Move Constructs.
         * @param[in] deleteTime  When should the product be deleted?
         * @param[in] prodId      The product's ID
         */
        DeleteEntry(
                const SysTimePoint&& deleteTime,
                const ProdId         prodId)
            : deleteTime(deleteTime)
            , prodId(prodId)
        {}

        /**
         * Indicates if this instance is less than another.
         * @param[in] rhs      The other instance
         * @retval    true     This instance is less than the other
         * @retval    false    This instance is not less than the other
         */
        bool operator<(const DeleteEntry& rhs) const {
            // `>` because `deleteQueue.top()` returns maximum entry
            return deleteTime > rhs.deleteTime;
        }
    };

    /// Type of map from product IDs to product entries
    using ProdEntries = std::unordered_map<ProdId, ProdEntry>;
    /// Type of container for product-files with open file descriptors
    using OpenProds   = HashSetQueue<ProdId>;
    /// Type of container for product-files that should be deleted after a certain time
    using DeleteQueue = std::priority_queue<DeleteEntry, std::deque<DeleteEntry>>;
    /// Type of container for information on data products
    using ProdQueue   = std::queue<ProdInfo>;

    mutable Mutex     mutex;          ///< To maintain consistency
    mutable Cond      cond;           ///< For inter-thread communication
    const SysDuration keepTime;       ///< Length of time to keep products
    ProdEntries       prodEntries;    ///< Product entries
    DeleteQueue       deleteQueue;    ///< Queue of products to delete and when to delete them
    OpenProds         openProds;      ///< Products whose files are open
    const size_t      maxOpenProds;   ///< Maximum number of open products
    const String      absPathRoot;    ///< Absolute pathname of root directory of repository
    size_t            rootPrefixLen;  ///< Length in bytes of root pathname prefix
    int               rootFd;         ///< File descriptor open on root-directory of repository
    size_t            maxOpenFiles;   ///< Max number open files
    ThreadEx          threadEx;       ///< Subtask exception
    ProdQueue         prodQueue;      ///< Queue of product metadata for external processing
    bool              stop;           ///< Whether or not to stop

    /// Stops the thread on which files are deleted
    void stopDeleteThread() {
        if (deleteThread.joinable()) {
            int status = ::pthread_cancel(deleteThread.native_handle());
            if (status)
                throw SYSTEM_ERROR("::pthread_cancel() failure on delete-thread");
            deleteThread.join();
        }
    }

    /**
     * Scans the repository. Adds products to the database.
     *
     * @param[in] absPathParent  Absolute pathname of directory to recursively scan
     * @throw InvalidArgument    File is not a directory or regular file
     * @throw SystemError        Couldn't get information on file
     * @throw SystemError        Couldn't change owner of file
     * @throw SystemError        Couldn't change mode of file
     */
    void scanRepo(const String& absPathParent) {
        LOG_ASSERT(FileUtil::isAbsolute(absPathParent));

        const int parentFd = ::open(absPathParent.data(), O_RDONLY | O_DIRECTORY);
        if (parentFd == -1)
            throw SYSTEM_ERROR("::open() failure on directory \"" + absPathParent + "\"");

        auto dir = ::fdopendir(parentFd);
        if (dir == nullptr)
            throw SYSTEM_ERROR("::opendir() failure on \"" + absPathParent + "\"; parentFd=" +
                    std::to_string(parentFd));

        try {
            struct dirent  entry;
            struct dirent* result;
            int            status;
            size_t         numAdded = 0;

            for (status = ::readdir_r(dir, &entry, &result);
                    status == 0 && result != nullptr;
                    status = ::readdir_r(dir, &entry, &result)) {
                const char* childFileName = entry.d_name;

                if (::strcmp(".", childFileName) == 0 || ::strcmp("..", childFileName) == 0)
                    continue;

                const std::string absPathChild = absPathParent + "/" + childFileName;
                ensurePrivate(absPathChild);

                struct stat       statBuf;
                FileUtil::statNoFollow(absPathChild, statBuf);

                if (S_ISDIR(statBuf.st_mode)) {
                    scanRepo(absPathChild);
                }
                else if (addExisting(absPathChild)) {
                    ++numAdded;
                }
            }
            if (status && status != ENOENT)
                throw SYSTEM_ERROR("Couldn't read directory \"" + absPathParent + "\"");

            ::closedir(dir); // NB: Closes `parentFd`

            LOG_DEBUG("Files added=" + std::to_string(numAdded));
        } // `dir` is initialized
        catch (...) {
            ::closedir(dir);
            throw;
        }
    }

    /// Sets the exception thrown by an internal thread
    void setThreadEx(const std::exception& ex) {
        Guard guard{mutex};
        threadEx.set(ex);
        cond.notify_all();
    }

    /**
     * Returns the product-name corresponding to the absolute pathname of a product-file.
     *
     * @param[in] pathname  Absolute pathname of a product-file
     * @return              Corresponding product-name
     */
    std::string getProdName(const std::string& pathname) const {
        LOG_ASSERT(FileUtil::isAbsolute(pathname));
        return pathname.substr(rootPrefixLen); // Remove `absPathRoot+"/"`
    }

    /**
     * Returns the absolute pathname of the file corresponding to a product name.
     *
     * @pre                 `prodName.size() > 0`
     * @param[in] prodName  Product name
     * @return              Absolute pathname of corresponding file
     */
    std::string getAbsPathname(const std::string& prodName) const
    {
        LOG_ASSERT(prodName.size());
        return absPathRoot + "/" + prodName;
    }

    /**
     * Ensures that a file is
     *   - Either a directory or a regular file;
     *   - Owned by this process' effective user and group; and
     *   - protected from all others.
     *
     * @param[in] absPathname  Absolute pathname of an existing file
     * @throw InvalidArgument  File is not a directory or regular file
     * @throw SystemError      Couldn't get information on file
     * @throw SystemError      Couldn't change owner of file
     * @throw SystemError      Couldn't change mode of file
     */
    void ensurePrivate(const String& absPathname) {
        struct ::stat statBuf;
        FileUtil::statNoFollow(absPathname, statBuf);

        if (!S_ISDIR(statBuf.st_mode) && !S_ISREG(statBuf.st_mode))
            throw INVALID_ARGUMENT("\"" + absPathname + "\" isn't a directory or regular file");

        if (statBuf.st_uid != ::geteuid())
            FileUtil::setOwnership(absPathname, ::geteuid(), ::getegid());

        const mode_t protMask = S_ISDIR(statBuf.st_mode) ? 0755 : 0644;
        if ((statBuf.st_mode & 0777) != protMask)
            FileUtil::setProtection(absPathname, protMask);
    }

    /**
     * Adds a product to the database from an existing product-file. This function will be called
     * within a publisher when a file is added to the repository.
     *
     * @pre                    Mutex is locked
     * @param[in] absPathname  Absolute pathname of existing file
     * @return                 A pair whose first component is a reference to the relevant
     *                         product-entry and whose second component is a boolean indicating if
     *                         the product was added (true) or rejected because it already existed
     *                         (false)
     * @post                   Mutex is locked
     */
    std::pair<ProdEntry&, bool> addExistingLocked(const String& absPathname) {
        LOG_ASSERT(!mutex.try_lock());
        LOG_ASSERT(FileUtil::isAbsolute(absPathname));
        LOG_ASSERT(absPathname.find(absPathRoot) == 0);

        ProdFile   prodFile{absPathname}; // Instance is closed
        const auto modTime = prodFile.getModTime();
        const auto prodName = absPathname.substr(rootPrefixLen);
        ProdInfo   prodInfo(prodName, prodFile.getProdSize(), modTime);
        ProdEntry  prodEntry{prodFile, prodInfo};
        auto       pair = prodEntries.emplace(prodInfo.getId(), prodEntry);
        const auto wasAdded = pair.second;

        if (pair.second) {
            auto&      prodFile = prodEntry.prodFile;
            const auto deleteTime = modTime + keepTime;

            LOG_TRACE("Adding to delete-queue: modTime=%s, deleteTime=%s",
                    std::to_string(modTime).data(), std::to_string(deleteTime).data());
            deleteQueue.push(DeleteEntry(deleteTime, prodInfo.getId()));
        }

        return {pair.first->second, pair.second};
    }

    /**
     * Activates a product-entry. Ensures that
     *   - The entry's product-file is open
     *   - The entry's product ID is at the back of the open-products queue
     *   - The size of the open-products queue is no more than `maxOpenProds`
     *
     * @pre                    Mutex is locked
     * @param[in] prodId       Product ID
     * @param[in] prodEntry    Product entry
     * @post                   Mutex is locked
     */
    void activate(const ProdId prodId, ProdEntry& prodEntry) {
        LOG_ASSERT(!mutex.try_lock());
        auto& prodFile = prodEntry.prodFile;
        openProds.erase(prodId); // Idempotent
        openProds.push(prodId);  // Idempotent

        while (openProds.size() > maxOpenProds) {
            prodEntries.at(openProds.front()).prodFile.close();
            openProds.pop();
        }
    }

    /**
     * Returns the product-entry in the database corresponding to a product-identifier. If found,
     * then the associated product-file is open and the product-identifier is at the end of the
     * open-products queue.
     *
     * @pre                  Mutex is locked
     * @param[in] prodId     Product-identifier
     * @return               Pointer to the product-entry
     * @retval    nullptr    No such entry
     * @post                 Mutex is locked
     */
    ProdEntry* findProdEntry(const ProdId prodId) {
        LOG_ASSERT(!mutex.try_lock());

        static ProdEntry invalid;
        if (prodEntries.count(prodId) == 0)
            return nullptr;

        ProdEntry& prodEntry = prodEntries[prodId];
        activate(prodId, prodEntry);

        return &prodEntry;
    }

public:
    /**
     * Constructs.
     *
     * @param[in] rootDir          Root directory of the repository
     * @param[in] maxOpenProds     Maximum number of products with open file descriptors
     * @param[in] keepTime         Duration to keep products before deleting them in seconds
     * @throw     InvalidArgument  `maxOpenProds <= 0`
     * @throw     InvalidArgument  `keepTime <= 0`
     * @throw     SystemError      Couldn't open root directory of repository
     */
    Impl(   const String& rootDir,
            const size_t  maxOpenProds,
            const int     keepTime)
        : deleteThread()
        , mutex()
        , cond()
        , keepTime(duration_cast<SysDuration>(seconds(keepTime)))
        , prodEntries()
        , deleteQueue()
        , openProds()
        , maxOpenProds{maxOpenProds}
        , absPathRoot(FileUtil::makeAbsolute(rootDir))
        , rootPrefixLen{absPathRoot.size() + 1}
        , rootFd(::open(FileUtil::ensureDir(absPathRoot).data(), O_DIRECTORY | O_RDONLY))
        , threadEx()
        , prodQueue()
        , stop(false)
    {
        if (maxOpenProds <= 0)
            throw INVALID_ARGUMENT("maxOpenProds=" + std::to_string(maxOpenProds));

        if (keepTime <= 0)
            throw INVALID_ARGUMENT("keepTime=" + std::to_string(keepTime));

        if (rootFd == -1)
            throw SYSTEM_ERROR("Couldn't open root-directory of repository, \"" + absPathRoot +
                    "\"");

        try {
            deleteThread = Thread(&Impl::deleteProds, this);
            LOG_ASSERT(deleteThread.joinable());
        } // `rootFd` is open
        catch (const std::exception& ex) {
            ::close(rootFd);
            throw;
        }
    }

    virtual ~Impl() {
        try {
            stopDeleteThread();

            if (rootFd >= 0)
                ::close(rootFd);
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    /**
     * Returns the absolute pathname of the root directory of the repository.
     * @return The absolute pathname of the root directory of the repository
     */
    const std::string& getRootDir() const noexcept
    {
        return absPathRoot;
    }

    /**
     * Returns the next product to process, either for transmission or local processing. Blocks
     * until one is available.
     * @return The next product to process. The product's metadata and data shall be complete.
     * @throws SystemError   System failure
     * @throws RuntimeError  inotify(7) failure
     */
    ProdEntry getNextProd() {
        ProdEntry prodEntry;
        Lock      lock(mutex);

        cond.wait(lock, [&]{return !prodQueue.empty() || threadEx;});

        threadEx.throwIfSet();

        prodEntry = *findProdEntry(prodQueue.front().getId());

        prodQueue.pop();

        return prodEntry;
    }

    /**
     * Returns information on the data product given its ID.
     * @param[in] prodId  The product's ID
     * @return            Information on the corresponding product. Will be invalid if the product
     *                    doesn't exist.
     */
    ProdInfo getProdInfo(const ProdId prodId) {
        threadEx.throwIfSet();

        static const ProdInfo empty{};
        Guard                 guard{mutex};
        ProdEntry*            prodEntry = findProdEntry(prodId);

        return prodEntry ? prodEntry->prodInfo : empty;
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
        threadEx.throwIfSet();

        static const DataSeg empty{};
        Guard                guard{mutex};
        ProdEntry*           prodEntry = findProdEntry(segId.prodId);

        return prodEntry
                ?  DataSeg{segId, prodEntry->prodFile.getProdSize(),
                        prodEntry->prodFile.getData(segId.offset)}
                :  empty;
    }

    /**
     * Returns the set of product identifiers comprising this instance's minus those of another set.
     *
     * @param[in]  rhs     Other set of product identifiers to be subtracted from this instance's
     * @return             This instance's product identifiers minus those of the other set
     */
    ProdIdSet subtract(const ProdIdSet rhs) {
        Guard     guard{mutex};
        ProdIdSet result(prodEntries.size() <= rhs.size()
                ? 0
                : prodEntries.size() - rhs.size());

        for (auto pair : prodEntries)
            if (rhs.count(pair.first) == 0)
                result.insert(pair.first);

        return result;
    }

    /**
     * Returns a set of identifiers for complete products.
     *
     * @return The set of complete product identifiers
     */
    ProdIdSet getProdIds() {
        Guard     guard{mutex};
        ProdIdSet prodIds(prodEntries.size());
        for (auto pair : prodEntries)
            prodIds.insert(pair.first);
        return prodIds;
    }

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

const std::string& Repository::getRootDir() const noexcept {
    return pImpl->getRootDir();
}

ProdEntry Repository::getNextProd() const {
    return pImpl->getNextProd();
}

ProdInfo Repository::getProdInfo(const ProdId prodId) const {
    return pImpl->getProdInfo(prodId);
}

DataSeg Repository::getDataSeg(const DataSegId segId) const {
    return pImpl->getDataSeg(segId);
}

ProdIdSet Repository::subtract(const ProdIdSet rhs) const {
    return pImpl->subtract(rhs);
}

ProdIdSet Repository::getProdIds() const {
    return pImpl->getProdIds();
}

/******************************************************************************/
/******************************************************************************/

/**
 * Implementation of a publisher's repository.
 */
class PubRepo::Impl final : public Repository::Impl
{
    Watcher            watcher;     ///< Watches the repository
    Thread             watchThread; ///< Thread for watching the repository

    static bool tryHardLink(
            const std::string& extantPath,
            const std::string& linkPath) {
        LOG_DEBUG("Linking \"" + linkPath + "\" to \"" + extantPath + "\"");
        FileUtil::ensureDir(FileUtil::dirname(linkPath), 0700);
        return ::link(extantPath.data(), linkPath.data()) == 0;
    }

    static void hardLink(
            const std::string& extantPath,
            const std::string& linkPath) {
        if (!tryHardLink(extantPath, linkPath))
            throw SYSTEM_ERROR("Couldn't link \"" + linkPath + "\" to \"" +
                    extantPath + "\"");
    }

    static bool trySoftLink(
            const std::string& extantPath,
            const std::string& linkPath) {
        LOG_DEBUG("Linking \"" + linkPath + "\" to \"" + extantPath + "\"");
        FileUtil::ensureDir(FileUtil::dirname(linkPath), 0700);
        return ::symlink(extantPath.data(), linkPath.data()) == 0;
    }

    static void softLink(
            const std::string& extantPath,
            const std::string& linkPath) {
        if (!trySoftLink(extantPath, linkPath))
            throw SYSTEM_ERROR("Couldn't link \"" + linkPath + "\" to \"" +
                    extantPath + "\"");
    }

    /**
     * Watches the repository. Adds new files as products to the database.
     */
    void watchRepo() {
        LOG_DEBUG("Watching repository %s for new product-files", absPathRoot.data());
        try {
            for (;;) {
                Watcher::WatchEvent event;
                watcher.getEvent(event);

                const auto& absPathname = event.pathname;
                LOG_ASSERT(FileUtil::isAbsolute(absPathname));
                LOG_ASSERT(absPathname.find(absPathRoot, 0) == 0);

                ensurePrivate(absPathname);

                {
                    Guard  guard(mutex);
                    Shield shield{};

                    auto pair = addExistingLocked(absPathname);
                    if (pair.second) {
                        auto& prodInfo = pair.first.prodInfo;
                        prodQueue.push(prodInfo);
                        cond.notify_all();
                    } // New product was added
                } // Mutex and shield are released
            } // Watch loop
        }
        catch (const std::exception& ex) {
            setThreadEx(ex);
        }
    }

public:
    /// Constructs
    Impl(   const String& rootPathname,
            const long    maxOpenFiles,
            const int     keepTime)
        : Repository::Impl{rootPathname, static_cast<size_t>(maxOpenFiles), keepTime}
        , watcher(absPathRoot)
        , watchThread{Thread{&Impl::watchRepo, this}}
    {
        try {
            scanRepo(absPathRoot);
        }
        catch (const std::exception& ex) {
            stopDeleteThread();
        }
    }

    ~Impl() {
        try {
            if (watchThread.joinable()) {
                int status = ::pthread_cancel(watchThread.native_handle());
                if (status)
                    LOG_SYSERR("::pthread_cancel() failure on watch-thread");
                watchThread.join();
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
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
            const String& pathname,
            const String& prodName)
    {
        threadEx.throwIfSet();

        if (pathname.size() == 0 || pathname[0] != '/')
            throw INVALID_ARGUMENT("Invalid pathname: \"" + pathname + "\"");
        if (prodName.size() == 0 || prodName[0] == '/')
            throw INVALID_ARGUMENT("Invalid product name: \"" + prodName + "\"");

        const std::string extantPath = FileUtil::makeAbsolute(pathname);
        const std::string linkPath = getAbsPathname(prodName);
        Guard             guard(mutex);

        FileUtil::ensureDir(FileUtil::dirname(linkPath), 0700);

        if (!tryHardLink(extantPath, linkPath) &&
                !trySoftLink(extantPath, linkPath))
            throw SYSTEM_ERROR("Couldn't link file \"" + linkPath + "\" to \"" + extantPath + "\"");
    }
};

/******************************************************************************/

PubRepo::PubRepo()
    : Repository{nullptr}
{}

PubRepo::PubRepo(
        const String& rootPathname,
        const long    maxOpenFiles,
        const int     keepTime)
    : Repository(new Impl(rootPathname, maxOpenFiles, keepTime))
{}

/******************************************************************************/
/******************************************************************************/

/**
 * Implementation of a subscriber's repository.
 */
class SubRepo::Impl final : public Repository::Impl
{
    static const String INCOMPLETE_DIR_NAME;

    /**
     * Returns a temporary absolute pathname to hold an incomplete data-product.
     *
     * @param[in] prodId  Product identifier
     * @return            Temporary absolute pathname
     */
    inline String tempAbsPathname(const ProdId prodId) {
        return absPathRoot + '/' + INCOMPLETE_DIR_NAME + '/' + prodId.to_string();
    }

    /**
     * Adds a product-entry. Upon success, the entry is open, at the back of the open-products list,
     * and in the delete-queue. The creation-time in the delete-queue is that of the product-
     * information in the product-entry if it's valid and the modification time of the product-file
     * otherwise.
     *
     * @param[in] prodId     Product-ID
     * @param[in] prodEntry  Valid product-entry. Product-information may be valid or invalid..
     * @return               The corresponding entry
     */
    ProdEntry& add(
            const ProdId prodId,
            ProdEntry&&  prodEntry) {
        LOG_ASSERT(!mutex.try_lock());
        LOG_ASSERT(prodEntry.prodFile || prodEntry.prodInfo);

        auto       pair = prodEntries.emplace(prodId, prodEntry);
        ProdEntry& entry = pair.first->second;

        if (pair.second) {
            SysTimePoint createTime = prodEntry.prodInfo
                    ? prodEntry.prodInfo.getCreateTime()
                    : prodEntry.prodFile.getModTime();
            deleteQueue.push(DeleteEntry{createTime+keepTime, prodId});
            activate(prodId, entry);
            cond.notify_all(); // State changed
        }

        return entry;
    }

    /**
     * Returns the product-entry corresponding to a product-ID. The product-entry is created if it
     * doesn't exist. Upon return, the corresponding product is open, at the back of the
     * open-products list, and in the delete-queue.
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

        ProdEntry* prodEntry = findProdEntry(prodId);

        if (prodEntry)
            return *prodEntry;

        ProdFile  prodFile{tempAbsPathname(prodId), prodSize};

        return add(prodId, ProdEntry{prodFile});
    }

    /**
     * Finishes processing a data-product if it's complete. The product is closed and its  metadata
     * is added to the outgoing-product queue.
     *
     * @pre                  State is locked
     * @param[in] prodEntry  Product entry
     * @post                 State is locked
     */
    void finishIfComplete(
            const ProdId prodId,
            ProdEntry&   prodEntry) {
        LOG_ASSERT(!mutex.try_lock());

        if (prodEntry.prodInfo) {
            auto& prodFile = prodEntry.prodFile;
            if (prodFile.isComplete()) {
                const auto& prodInfo = prodEntry.prodInfo;
                const auto& modTime = prodInfo.getCreateTime();
                prodFile.setModTime(modTime);

                const auto newPathname = absPathRoot + '/' + prodInfo.getName();
                FileUtil::ensureDir(FileUtil::dirname(newPathname), 0755);
                prodFile.rename(newPathname);

                prodFile.close();
                openProds.erase(prodId);
                prodQueue.push(prodEntry.prodInfo);
                cond.notify_all();
            }
        }
    }

public:
    Impl(   const std::string& rootPathname,
            const size_t       maxOpenFiles,
            const int          keepTime)
        : Repository::Impl{rootPathname, maxOpenFiles, keepTime}
    {
        try {
            /**
             * Ignore incomplete data-products and start from scratch. They will be completely
             * recovered.
             */
            FileUtil::rmDirTree(absPathRoot + '/' + INCOMPLETE_DIR_NAME);
            scanRepo(absPathRoot); // NB: No "incomplete" directory for it to scan
        }
        catch (const std::exception& ex) {
            stopDeleteThread();
        }
    }

    /**
     * Saves product-information. Creates a new product if necessary. If the resulting product
     * becomes complete, then it will be eventually returned by `getNextProd()`.
     *
     * @param[in] prodInfo     Product information
     * @retval    true         Product information was saved
     * @retval    false        Product information was previously saved
     * @throw InvalidArgument  Known product has a different size
     * @throw LogicError       Metadata was previously saved
     * @throw SystemError      System failure
     * @see `getNextProd()`
     */
    bool save(const ProdInfo prodInfo) {
        threadEx.throwIfSet();

        bool       wasSaved = false;
        const auto prodId = prodInfo.getId();
        Guard      guard{mutex};
        ProdEntry* prodEntry = findProdEntry(prodId);

        if (prodEntry) {
            // Entry is open and appended to open-list
            if (!prodEntry->prodInfo) {
                prodEntry->prodInfo = prodInfo;
                finishIfComplete(prodId, *prodEntry);
                wasSaved = true;
            }
        }
        else {
            String   pathname = tempAbsPathname(prodId);
            ProdFile prodFile(pathname, prodInfo.getSize()); // Is open
            ProdEntry& prodEntry = add(prodId, ProdEntry{prodFile, prodInfo});
            finishIfComplete(prodId, prodEntry); // Supports products with no data
            wasSaved = true;
        }

        return wasSaved;
    }

    /**
     * Saves a data-segment. If the resulting product becomes complete, then it will be eventually
     * returned by `getNextProd()`.
     *
     * @param[in] dataSeg      Data segment
     * @retval    true         This item was saved
     * @retval    false        This item was not saved because it already exists
     * @throw InvalidArgument  Known product has a different size
     * @throw SystemError      System failure
     * @see `getNextProd()`
     */
    bool save(const DataSeg dataSeg)
    {
        threadEx.throwIfSet();

        bool       wasSaved = false;
        const auto prodId = dataSeg.getId().prodId;
        Guard      guard{mutex};
        ProdEntry* prodEntry = findProdEntry(prodId);

        if (prodEntry) {
            if (prodEntry->prodFile.save(dataSeg)) {
                finishIfComplete(prodId, *prodEntry);
                wasSaved = true;
            }
        }
        else {
            // Product hasn't been previously seen => product-information can't be valid
            String   pathname = tempAbsPathname(prodId);
            ProdFile prodFile{pathname, dataSeg.getProdSize()}; // Is open
            prodFile.save(dataSeg);
            add(prodId, ProdEntry{prodFile});
            wasSaved = true;
        }

        return wasSaved;
    }

    /**
     * Indicates if a data-product's metadata exists.
     *
     * @param[in] prodId     Product identifier
     * @retval    true       Product metadata does exist
     * @retval    false      Product metadata does not exist
     */
    bool exists(const ProdId prodId)
    {
        threadEx.throwIfSet();

        Guard      guard{mutex};
        ProdEntry* prodEntry = findProdEntry(prodId);

        return prodEntry && prodEntry->prodInfo;
    }

    /**
     * Indicates if a data-segment exists.
     *
     * @param[in] segId      Data-segment identifier
     * @retval    true       Data-segment does exist
     * @retval    false      Data-segment does not exist
     */
    bool exists(const DataSegId segId)
    {
        threadEx.throwIfSet();

        Guard      guard{mutex};
        ProdEntry* prodEntry = findProdEntry(segId.prodId);

        return prodEntry && prodEntry->prodFile.exists(segId.offset);
    }
};

const String SubRepo::Impl::INCOMPLETE_DIR_NAME = ".incomplete";

/**************************************************************************************************/

SubRepo::SubRepo()
    : Repository{nullptr}
{}

SubRepo::SubRepo(
        const std::string& rootPathname,
        const size_t       maxOpenFiles,
        const int          keepTime)
    : Repository(new Impl(rootPathname, maxOpenFiles, keepTime))
{}

bool SubRepo::save(const ProdInfo prodInfo) const {
    return static_cast<Impl*>(pImpl.get())->save(prodInfo);
}

bool SubRepo::save(const DataSeg dataSeg) const {
    return static_cast<Impl*>(pImpl.get())->save(dataSeg);
}

bool SubRepo::exists(const ProdId prodId) const {
    return static_cast<Impl*>(pImpl.get())->exists(prodId);
}

bool SubRepo::exists(const DataSegId segId) const {
    return static_cast<Impl*>(pImpl.get())->exists(segId);
}

} // namespace
