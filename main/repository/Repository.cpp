/**
 * Repository of data-products.
 *
 *        File: Repository.cpp
 *  Created on: Dec 23, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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
#include "Thread.h"
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
     * Deletes products that are at least as old as the keep-time. Implemented as a start routine
     * for a separate thread that's expected to be cancelled. Calls `setThreadEx()` on exception.
     */
    void deleteProds() {
        try {
            /**
             * The following code deletes products on-time regardless of their arrival rate.
             */
            auto pred = [&] {return (deleteQueue.top().deleteTime <= SysClock::now());};
            Lock lock{mutex};

            for (;;) {
                /*
                 * The following appears necessary to ensure that processing proceeds as soon as it
                 * can.
                 */
                if (deleteQueue.empty())
                    cond.wait(lock, [&]{return !deleteQueue.empty();});
                cond.wait_until(lock, deleteQueue.top().deleteTime, pred);

                // Temporarily disable thread cancellation to protect the following state change
                Shield     shield{};
                const auto prodId = deleteQueue.top().prodId;
                auto       iter = prodEntries.find(ProdEntry{prodId});

                if (iter != prodEntries.end()) {
                    auto& prodFile = iter->prodFile;
                    /*
                     * The product-file isn't closed in order to allow concurrent access to the
                     * data-product by the node for transmission or local processing.
                     */
                    FileUtil::removeFileAndPrune(absPathRoot, prodFile.getPathname());
                    LOG_DEBUG("Deleted old file \"" + prodFile.getPathname() + "\"");
                    openProds.erase(prodId);
                    prodEntries.erase(iter);
                    deleteQueue.pop();
                }
            }

            LOG_DEBUG("Deleted too-old products");
        }
        catch (const std::exception& ex) {
           /*
            * SystemError  Couldn't delete file
            * SystemError  Couldn't delete empty directory
            */
            setThreadEx();
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

    /// Class function to return the hash value of a product-entry
    struct HashProdEntry {
        /**
         * Returns the hash value of a product-entry.
         * @param[in] prodEntry  Product-entry
         * @return               Corresponding hash value
         */
        size_t operator()(const ProdEntry& prodEntry) const noexcept {
            return prodEntry.hash();
        }
    };

    /// Set of product entries
    using ProdEntries = std::unordered_set<ProdEntry, HashProdEntry>;
    /// Container for product-files with open file descriptors
    using ProdIdQueue = HashSetQueue<ProdId>;
    /// Container for product-files that should be deleted after a certain time
    using DeleteQueue = std::priority_queue<DeleteEntry, std::deque<DeleteEntry>>;
    /// Container for information on data products
    using ProcQueue   = std::queue<ProdId>;

    mutable Mutex     mutex;          ///< To maintain consistency
    mutable Cond      cond;           ///< For inter-thread communication
    const SysDuration keepTime;       ///< Length of time to keep products
    ProdEntries       prodEntries;    ///< Product entries
    DeleteQueue       deleteQueue;    ///< Queue of products to delete and when to delete them
    ProdIdQueue       openProds;      ///< Products whose files are open
    const size_t      maxOpenProds;   ///< Maximum number of open products
    const String      absPathRoot;    ///< Absolute pathname of root directory of repository.
                                      ///< Includes trailing separator.
    size_t            rootPrefixLen;  ///< Length in bytes of root pathname prefix. Includes
                                      ///< trailing separator.
    int               rootFd;         ///< File descriptor open on root-directory of repository
    ThreadEx          threadEx;       ///< Subtask exception
    ProcQueue         procQueue;      ///< Queue of identifiers of products ready for processing

    /**
     * Ensures that a repository pathname is relative. If the pathname is prefixed with the
     * repository's root directory, then the prefix is removed. If the pathname is already relative,
     * then it is unchanged.
     * @param[in,out] pathname  Pathname to be made relative
     * @throw InvalidArgument   `pathname` is absolute but doesn't have the repository's root
     *                          directory as a prefix
     */
    void makeRelative(String& pathname) const {
        if (pathname.find(absPathRoot) == 0) {
            pathname.erase(0, rootPrefixLen);
        }
        else if (FileUtil::isAbsolute(pathname)) {
            throw INVALID_ARGUMENT("Absolute pathname not part of repository");
        }
    }

    /**
     * Ensures that a repository pathname is absolute. If the pathname is relative, then it is
     * prefixed with the repository's root directory. If the pathname is already prefixed with the
     * repository's root directory, then it is unchanged.
     * @param[in,out] pathname  Pathname to be made absolute
     * @throw InvalidArgument   `pathname` is absolute but doesn't have the repository's root
     *                          directory as a prefix
     */
    void makeAbsolute(String& pathname) const {
        if (pathname.find(absPathRoot) != 0) {
            if (FileUtil::isAbsolute(pathname))
                throw INVALID_ARGUMENT("Absolute pathname not part of repository");
            pathname.insert(0, absPathRoot);
        }
    }

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
     * Process a directory in the repository. This default implementation does nothing.
     *
     * @param[in] absPathname  Absolute pathname of existing directory
     */
    virtual void processDir(const String& absPathname) {
    }

    /**
     * Tries to add an existing, regular file in the repository to the database. If the file is too
     * old, then it's deleted; otherwise, it's added to the database as a product-file if it's not
     * already there. This function is called by both publishers and subscribers during the initial
     * scan of the repository and by the publisher when a file is added to the repository.
     *
     * @pre                    Mutex is locked
     * @param[in] absPathname  Absolute pathname of existing, regular file
     * @retval    true         File was added
     * @retval    false        File was not added because it's a duplicate or was too old
     * @post                   Mutex is locked
     */
    bool tryAddFile(const String& absPathname) {
        LOG_ASSERT(!mutex.try_lock());
        LOG_ASSERT(absPathname.find(absPathRoot) == 0);

        ProdFile   prodFile{absPathname}; // Instance is closed
        const auto modTime = prodFile.getModTime();
        const auto deleteTime = modTime + keepTime;
        auto       wasAdded = false;

        if (deleteTime < SysClock::now()) {
            FileUtil::removeFileAndPrune(absPathRoot, absPathname);
            LOG_DEBUG("Deleted old file \"" + prodFile.getPathname() + "\"");
        }
        else {
            const auto prodName = absPathname.substr(rootPrefixLen);
            ProdInfo   prodInfo(prodName, prodFile.getProdSize(), modTime);
            ProdEntry  prodEntry{prodInfo, prodFile};
            auto       pair = prodEntries.emplace(prodInfo, prodFile);

            if (pair.second) {
                deleteQueue.push(DeleteEntry(deleteTime, prodInfo.getId()));
                LOG_TRACE("Added to delete-queue: modTime=%s, deleteTime=%s",
                        std::to_string(modTime).data(), std::to_string(deleteTime).data());
                wasAdded = true;
            }
        }

        return wasAdded;
    }

    /**
     * Recursively scans a directory in the repository. Calls `processDir()` on directories and
     * adds regular files to the database.
     *
     * @param[in] absPathParent  Absolute pathname of a repository directory to recursively scan
     * @throw SystemError        Couldn't open `absPathParent`
     * @throw SystemError        Couldn't get information on file
     * @throw SystemError        Couldn't change owner of file
     * @throw SystemError        Couldn't change mode of file
     * @see processDir()
     */
    void scanRepo(const String& absPathParent) {
        LOG_ASSERT(FileUtil::isAbsolute(absPathParent));

        auto dir = ::opendir(absPathParent.data());
        if (dir == nullptr)
            throw SYSTEM_ERROR("::opendir() failure on directory \"" + absPathParent + "\"");

        try {
            processDir(absPathParent);

            struct dirent  entry;
            struct dirent* result;
            int            status;
            size_t         numAdded = 0;

            for (status = ::readdir_r(dir, &entry, &result);
                    status == 0 && result != nullptr;
                    status = ::readdir_r(dir, &entry, &result)) {
                const String childFileName(entry.d_name);

                if (childFileName == "." || childFileName == "..")
                    continue;

                const String absPathChild = FileUtil::pathname(absPathParent, childFileName);
                ensurePrivate(absPathChild);

                struct stat  statBuf;
                FileUtil::statNoFollow(absPathChild, statBuf);

                if (S_ISDIR(statBuf.st_mode)) {
                    scanRepo(absPathChild);
                }
                else {
                    Guard guard{mutex};
                    if (tryAddFile(absPathChild)) {
                        ++numAdded;
                        cond.notify_all();
                    }
                }
            }
            if (status && status != ENOENT)
                throw SYSTEM_ERROR("Couldn't read directory \"" + absPathParent + "\"");

            ::closedir(dir);

            LOG_DEBUG("Files added=" + std::to_string(numAdded));
        } // `dir` is initialized
        catch (...) {
            ::closedir(dir);
            throw;
        }
    }

    /// Sets the exception thrown by an internal thread
    void setThreadEx() {
        Guard guard{mutex};
        threadEx.set();
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
        return pathname.substr(rootPrefixLen); // Remove `absPathRoot`+separator
    }

    /**
     * Returns the absolute pathname of the file corresponding to a product name.
     *
     * @pre                     `prodName.size() > 0`
     * @param[in] prodName      Product name
     * @return                  Absolute pathname of corresponding file
     * @throw InvalidArgument   `prodName` is absolute
     */
    std::string getAbsPathname(const std::string& prodName) const
    {
        LOG_ASSERT(prodName.size());
        String absPathname(prodName);
        makeAbsolute(absPathname);
        return absPathname;
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
     * Activates a product-entry. Ensures that
     *   - The entry's product-file is open
     *   - The entry's product ID is at the back of the open-products queue
     *   - The size of the open-products queue is no more than `maxOpenProds`
     *
     * @pre                    Mutex is locked
     * @param[in] prodEntry    Product entry
     * @post                   Mutex is locked
     */
    void activate(const ProdEntry& prodEntry) {
        LOG_ASSERT(!mutex.try_lock());

        prodEntry.prodFile.getData(); // Ensures that the product-file is open

        const auto  prodId = prodEntry.prodInfo.getId();
        openProds.erase(prodId); // Idempotent
        openProds.push(prodId);  // Idempotent

        while (openProds.size() > maxOpenProds) {
            prodEntries.find(ProdEntry{openProds.front()})->prodFile.close();
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
     * @return               Iterator to the product-entry or `prodEntries.end()`
     * @post                 Mutex is locked
     */
    ProdEntries::iterator getProdEntry(const ProdId prodId) {
        LOG_ASSERT(!mutex.try_lock());

        auto iter = prodEntries.find(ProdEntry{prodId});
        if (iter != prodEntries.end())
            activate(*iter);

        return iter;
    }

public:
    /**
     * Constructs.
     *
     * @param[in] repoDir          Pathname of repository directory
     * @param[in] maxOpenProds     Maximum number of products with open file descriptors
     * @param[in] keepTime         Number of seconds to keep products before deleting them
     * @throw     InvalidArgument  `maxOpenProds <= 0`
     * @throw     InvalidArgument  `keepTime <= 0`
     * @throw     SystemError      Couldn't open repository directory
     */
    Impl(   const String& repoDir,
            const size_t  maxOpenProds,
            const int     keepTime)
        : deleteThread()
        , mutex()
        , cond()
        , keepTime(duration_cast<SysDuration>(seconds(keepTime)))
        , prodEntries(1000)
        , deleteQueue()
        , openProds()
        , maxOpenProds{maxOpenProds}
        , absPathRoot(FileUtil::ensureTrailingSep(FileUtil::makeAbsolute(repoDir)))
        , rootPrefixLen{absPathRoot.size()}
        , rootFd(::open(FileUtil::ensureDir(absPathRoot).data(), O_DIRECTORY | O_RDONLY))
        , threadEx()
        , procQueue()
    {
        if (maxOpenProds <= 0)
            throw INVALID_ARGUMENT("maxOpenProds=" + std::to_string(maxOpenProds));

        if (keepTime <= 0)
            throw INVALID_ARGUMENT("keepTime=" + std::to_string(keepTime));

        if (rootFd == -1)
            throw SYSTEM_ERROR("Couldn't open repository directory, \"" + absPathRoot + "\"");

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
     * until one is available. The returned product is active.
     * @return The next product to process. The product's metadata and data shall be complete.
     * @throws SystemError   System failure
     * @throws RuntimeError  inotify(7) failure
     */
    ProdEntry getNextProd() {
        ProdEntry prodEntry;
        Lock      lock(mutex);

        for (;;) {
            cond.wait(lock, [&]{return !procQueue.empty() || threadEx;});

            threadEx.throwIfSet();

            auto iter = getProdEntry(procQueue.front());

            if (iter != prodEntries.end()) {
                prodEntry = *iter;
                openProds.erase(iter->prodInfo.getId());
                procQueue.pop();
                break;
            }
        }

        return prodEntry;
    }

    /**
     * Returns information on a data product given its ID.
     * @param[in] prodId  The product's ID
     * @return            Information on the corresponding product. Will test false if the product
     *                    doesn't exist.
     */
    ProdInfo getProdInfo(const ProdId prodId) {
        threadEx.throwIfSet();

        static const ProdInfo invalid{};
        Guard                 guard{mutex};
        auto                  iter = getProdEntry(prodId);

        return iter == prodEntries.end() ? invalid : iter->prodInfo;
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
    DataSeg getDataSeg(const DataSegId segId) {
        threadEx.throwIfSet();

        static const DataSeg invalid{};
        Guard                guard{mutex};
        auto                 iter = getProdEntry(segId.prodId);

        return iter != prodEntries.end()
                ?  DataSeg{segId, iter->prodFile.getProdSize(),
                        iter->prodFile.getData(segId.offset)}
                :  invalid;
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

        for (auto prodEntry : prodEntries) {
            const auto prodId = prodEntry.prodInfo.getId();
            if (rhs.count(prodId) == 0)
                result.insert(prodId);
        }

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
        for (auto prodEntry : prodEntries)
            prodIds.insert(prodEntry.prodInfo.getId());
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
class PubRepo::Impl final : public Repository::Impl, public Watcher::Client
{
    Watcher watcher;     ///< Watches the repository
    Thread  watchThread; ///< Thread to execute the Watcher

    void runWatcher() {
        try {
            watcher();
        }
        catch (const std::exception& ex) {
            setThreadEx();
        }
    }

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

protected:
    /**
     * Process a directory in the repository. The directory is given to the watcher to be added to
     * the set of watched directories.
     *
     * @param[in] dirPath  Absolute pathname of an existing repository directory
     */
    void processDir(const String& dirPath) {
        LOG_ASSERT(dirPath.find(absPathRoot) == 0);
        watcher.tryAdd(dirPath);
    }

public:
    /**
     * Constructs. Upon return, the repository has been completely scanned and all relevant
     * directories are being watched.
     * @param[in] rootPathname  Pathname of the repository
     * @param[in] maxOpenFiles  Maximum number of files to have open simultaneously
     * @param[in] keepTime      Number of seconds to keep products before deleting them
     */
    Impl(   const String& rootPathname,
            const long    maxOpenFiles,
            const int     keepTime)
        : Repository::Impl{rootPathname, static_cast<size_t>(maxOpenFiles), keepTime}
        , watcher(absPathRoot, *this)
        , watchThread(&Impl::runWatcher, this)
    {
        /**
         * Ideally, the following sequence would occur:
         *   - The repository would be scanned completely and previously-existing files added or
         *     deleted;
         *   - The watcher would be started; then
         *   - New files would added.
         * Unfortunately, the addition of new files is outside of this application's control (and
         * could occur during this constructor), yet newly-added files and pre-existing files must
         * be distinguished because only the newly-added ones need to be reliably multicast
         * immediately. Consequently, both the watcher and the scanning for previously-existing
         * files are executed in parallel on separate threads. As a consequence, a newly-added file
         * might be acted upon by both threads. Duplicate detection, however, will ensure that such
         * files are added to the database only once.
         */
        try {
            scanRepo(absPathRoot);
        }
        catch (const std::exception& ex) {
            stopDeleteThread();
        }
    }

    ~Impl() {
        if (watchThread.joinable()) {
            ::pthread_cancel(watchThread.native_handle());
            watchThread.join();
        }
    }

    /**
     * Processes the creation of a directory in the repository. Called by the Watcher.
     * @param[in] pathname  Absolute pathname of the directory
     */
    void dirAdded(const String& pathname) override {
        processDir(pathname);
    }

    /**
     * Processes the addition of a regular file to the repository. Called by the Watcher.
     * @param[in] pathname  Absolute pathname of the regular file
     */
    void fileAdded(const String& pathname) override {
        LOG_ASSERT(pathname.find(absPathRoot) == 0);

        Guard guard{mutex};
        if (tryAddFile(pathname)) {
            /*
             * The newly-added file was added to the database. It must now be added to the queue of
             * files to be processed.
             */
            const auto prodName = pathname.substr(rootPrefixLen);
            procQueue.push(ProdId(prodName));
            cond.notify_all();
        }
    }

    /**
     * Links to a file (which could be a directory) that's outside the repository. The watcher will
     * notice and process the link.
     *
     * @param[in] pathname       Absolute pathname (with no trailing separator) of the file or
     *                           directory to be linked to
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

        if (pathname.size() == 0 || pathname[0] != FileUtil::separator)
            throw INVALID_ARGUMENT("Invalid pathname: \"" + pathname + "\"");
        if (prodName.size() == 0 || prodName[0] == FileUtil::separator)
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
        return FileUtil::pathname(FileUtil::pathname(absPathRoot, INCOMPLETE_DIR_NAME),
                prodId.to_string());
    }

    /**
     * Adds a product-entry. Upon success, the entry is open, at the back of the open-products list,
     * and in the delete-queue. The delete-time in the delete-queue is based on the creation time in
     * the product-information in the product-entry if it's valid and the modification time of the
     * product-file otherwise.
     *
     * @param[in] prodEntry  Product-entry
     * @return               The corresponding entry
     */
    const ProdEntry& add(ProdEntry&& prodEntry) {
        LOG_ASSERT(!mutex.try_lock());

        auto pair = prodEntries.emplace(prodEntry);
        auto iter = pair.first;

        if (pair.second) {
            auto& prodInfo = iter->prodInfo;
            SysTimePoint createTime = prodInfo
                    ? prodInfo.getCreateTime()
                    : iter->prodFile.getModTime();
            deleteQueue.push(DeleteEntry{createTime+keepTime, prodInfo.getId()});
            activate(*iter);
            cond.notify_all(); // State changed
        }

        return *iter;
    }

    /**
     * Finishes processing a data-product if it's complete. The product is closed and its metadata
     * is added to the product-queue for local processing.
     *
     * @pre                  State is locked
     * @param[in] prodEntry  Product entry
     * @post                 State is locked
     */
    void tryFinnish(const ProdEntry& prodEntry) {
        LOG_ASSERT(!mutex.try_lock());

        if (prodEntry.prodInfo) {
            auto& prodFile = prodEntry.prodFile;
            if (prodFile.isComplete()) {
                const auto& prodInfo = prodEntry.prodInfo;
                const auto  prodId = prodInfo.getId();
                const auto& modTime = prodInfo.getCreateTime();
                prodFile.setModTime(modTime);

                LOG_INFO("Received product " + prodInfo.to_string());

                const auto newPathname = absPathRoot + prodInfo.getName();
                FileUtil::ensureDir(FileUtil::dirname(newPathname), 0755);
                prodFile.rename(newPathname);

                prodFile.close(); // Removal from product-queue for processing will re-enable access
                openProds.erase(prodId);
                procQueue.push(prodInfo.getId());
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
            FileUtil::rmDirTree(absPathRoot + INCOMPLETE_DIR_NAME);
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
        auto       iter = getProdEntry(prodId);

        if (iter != prodEntries.end()) {
            // Entry is open and appended to open-list
            if (!iter->prodInfo) {
                *const_cast<ProdInfo*>(&iter->prodInfo) = prodInfo; // Ok because same ProdId
                tryFinnish(*iter);
                wasSaved = true;
            }
        }
        else {
            String           pathname = tempAbsPathname(prodId);
            ProdFile         prodFile(pathname, prodInfo.getSize());
            const ProdEntry& prodEntry = add(ProdEntry{prodInfo, prodFile});

            tryFinnish(prodEntry); // Supports products with no data
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
        auto       iter = getProdEntry(prodId);

        if (iter != prodEntries.end()) {
            if (iter->prodFile.save(dataSeg)) {
                tryFinnish(*iter);
                wasSaved = true;
            }
        }
        else {
            // Product hasn't been previously seen => product-information can't be valid
            String   pathname = tempAbsPathname(prodId);
            ProdFile prodFile{pathname, dataSeg.getProdSize()}; // Is open
            prodFile.save(dataSeg);
            add(ProdEntry{prodId, prodFile});
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

        Guard  guard{mutex};
        auto   iter = getProdEntry(prodId);

        return iter != prodEntries.end() && iter->prodInfo;
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

        Guard guard{mutex};
        auto  iter = getProdEntry(segId.prodId);

        return iter != prodEntries.end() && iter->prodFile.exists(segId.offset);
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
