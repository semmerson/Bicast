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

#include "BicastProto.h"
#include "CommonTypes.h"
#include "FileUtil.h"
#include "HashSetQueue.h"
#include "LastProd.h"
#include "logging.h"
#include "Shield.h"
#include "Thread.h"
#include "ThreadException.h"
#include "Watcher.h"

#include <atomic>
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

namespace bicast {

using namespace std::chrono;

/**
 * Abstract, base implementation of a repository of transient data-products.
 */
class Repository::Impl
{
    /**
     * Deletes products that are at least as old as the keep-time. Implemented as a start routine
     * for a separate thread. Calls `setThreadEx()` on exception.
     */
    void deleteProds() {
        try {
            //LOG_DEBUG("Starting deleter of products that are too old");
            /**
             * The following code deletes products on-time regardless of their arrival rate.
             */
            auto doneOrNotEmpty= [&]{return done || !deleteQueue.empty();};
            auto doneOrReady= [&] {return done || deleteQueue.top().deleteTime <= SysClock::now();};
            Lock lock{mutex};

            for (;;) {
                /*
                 * The following appears necessary to ensure that processing proceeds as soon as it
                 * can.
                 */
                if (deleteQueue.empty())
                    deleteQueueCond.wait(lock, doneOrNotEmpty);
                deleteQueueCond.wait_until(lock, deleteQueue.top().deleteTime, doneOrReady);
                if (done)
                    break;

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
                    FileUtil::removeFileAndPrune(prodDir, prodFile.getPathname());
                    LOG_DEBUG("Deleted old file \"" + prodFile.getPathname() + "\"");
                    openProds.erase(prodId);
                    prodEntries.erase(iter);
                    deleteQueue.pop();
                }
            }

            //LOG_DEBUG("Deleted too-old products");
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
    /// Queue of product-files with open file descriptors. LRU is at the front.
    using ProdIdQueue = HashSetQueue<ProdId>;
    /// Queue for product-files that should be deleted after a certain time
    using DeleteQueue = std::priority_queue<DeleteEntry, std::deque<DeleteEntry>>;
    /// Queue for local processing of data products
    using ProcQueue   = std::map<SysTimePoint, ProdId>;

    mutable Mutex     mutex;           ///< To maintain consistency
    bool              stop;            ///< For stopping getNextProd()
    mutable Cond      procQueueCond;   ///< For waiting on the process-queue
    mutable Cond      deleteQueueCond; ///< For waiting on the delete-queue
    const SysDuration keepTime;        ///< Length of time to keep products
    ProdEntries       prodEntries;     ///< Product entries
    DeleteQueue       deleteQueue;     ///< Queue of products to delete and when to delete them
    ProdIdQueue       openProds;       ///< Products whose files are open. LRU is at the front.
    const size_t      maxOpenProds;    ///< Maximum number of products with an open file descriptor
    const String      prodDir;         ///< Absolute pathname of directory containing complete
                                       ///< product-files. Includes trailing separator.
    size_t            prodDirLen;      ///< Byte-length of `prodDir`. Includes trailing separator.
    int               prodDirFd;       ///< File descriptor open on `prodDir`
    ThreadEx          threadEx;        ///< Subtask exception
    ProcQueue         procQueue;       ///< Queue of identifiers of products ready for processing
                                       ///< (i.e., either being transmitted or locally processed)
    SysTimePoint      lastProcessed;   ///< Time of the last, successfully-processed file
    bool              done;            ///< Should this instance terminate?

    /**
     * Ensures that a repository pathname is relative. If the pathname is prefixed with the
     * repository's root directory, then the prefix is removed. If the pathname is already relative,
     * then it is unchanged.
     * @param[in,out] pathname  Pathname to be made relative
     * @throw InvalidArgument   `pathname` is absolute but doesn't have the repository's root
     *                          directory as a prefix
     */
    void makeRelative(String& pathname) const {
        if (pathname.find(prodDir) == 0) {
            pathname.erase(0, prodDirLen);
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
        if (pathname.find(prodDir) != 0) {
            if (FileUtil::isAbsolute(pathname))
                throw INVALID_ARGUMENT("Absolute pathname not part of repository");
            pathname.insert(0, prodDir);
        }
    }

    /**
     * Maybe adds a directory to the repository's watch-list. This default implementation does
     * nothing.
     *
     * @param[in] absPathname  Absolute pathname of existing directory in the repository
     */
    virtual void maybeWatch(const String& absPathname) {
    }

    /**
     * Tries to add an existing, regular file in the repository to the database. If the file is too
     * old, then it's deleted and its ancestor directories pruned; otherwise, it's added to the
     * database as a product-file if it's not already there. This function is called by both
     * publishers and subscribers during the initial scan of the repository and by the publisher
     * when a file is added to the repository.
     *
     * @pre                    Mutex is locked
     * @param[in] absPathname  Absolute pathname of existing, regular file
     * @retval    true         File was added
     * @retval    false        File was not added because it's a duplicate or was too old
     * @post                   Mutex is locked
     */
    bool tryAddFile(const String& absPathname) {
        LOG_ASSERT(!mutex.try_lock());
        LOG_ASSERT(absPathname.find(prodDir) == 0);

        ProdFile   prodFile{absPathname}; // Instance is closed
        const auto modTime = prodFile.getModTime();
        const auto deleteTime = modTime + keepTime;
        auto       wasAdded = false;

        if (deleteTime < SysClock::now()) {
            FileUtil::removeFileAndPrune(prodDir, absPathname);
            LOG_DEBUG("Deleted old file \"" + prodFile.getPathname() + "\"");
        }
        else {
            const auto prodName = getProdName(absPathname);
            ProdInfo   prodInfo(prodName, prodFile.getProdSize(), modTime);
            ProdEntry  prodEntry{prodInfo, prodFile};

            if (prodEntries.emplace(prodInfo, prodFile).second) {
                // Product-file is not a duplicate
                deleteQueue.push(DeleteEntry(deleteTime, prodInfo.getId()));
                deleteQueueCond.notify_all();
                LOG_TRACE("Added to delete-queue: modTime=%s, deleteTime=%s",
                        std::to_string(modTime).data(), std::to_string(deleteTime).data());

                if (modTime > lastProcessed) {
                    // Product was created after that of the last, successfully-processed product
                    procQueue.emplace(modTime, prodInfo.getId());
                    procQueueCond.notify_all();
                }

                wasAdded = true;
            }
        }

        return wasAdded;
    }

    /**
     * Recursively scans a directory in the repository. Calls processFile() on regular files.
     *
     * General:
     *   - A directory is scanned
     *     - An encountered regular file is:
     *       - Deleted if it's older than the keep-time, and its directory is pruned (up to but not
     *         including the root directory)
     *       - Otherwise:
     *         - Added to the database
     *         - Added to the process-queue iff it's newer than the last, successfully-processed
     *           file from the previous session
     *     - An encountered directory is:
     *       - Added to the publisher's watch-list if this is the publisher's repository
     *       - Recursively scanned
     * Publisher only:
     *   - A file from the watcher is:
     *     - Deleted if it's older than the keep-time and its directory is pruned (up to but not
     *       including the root directory)
     *     - Otherwise:
     *       - Added to the database
     *       - Added to the process-queue
     *   - A directory from the watcher is:
     *     - Added to the watch-list
     *     - Recursively scanned
     *
     * @param[in] absPathParent  Absolute pathname of a repository directory to recursively scan
     * @throw SystemError        Couldn't open `absPathParent`
     * @throw SystemError        Couldn't get information on file
     * @throw SystemError        Couldn't change owner of file
     * @throw SystemError        Couldn't change mode of file
     * @see processFile()
     */
    void scanRepo(const String& absPathParent) {
        LOG_ASSERT(FileUtil::isAbsolute(absPathParent));

        auto dir = ::opendir(absPathParent.data());
        if (dir == nullptr)
            throw SYSTEM_ERROR("::opendir() failure on directory \"" + absPathParent + "\"");

        try {
            maybeWatch(absPathParent);

            struct dirent  entry;
            struct dirent* result;
            int            status;
            size_t         numAdded = 0;

            while ((status = ::readdir_r(dir, &entry, &result)) == 0 && result != nullptr) {
                const String childFileName(entry.d_name);

                if (childFileName == "." || childFileName == "..")
                    continue;

                const String absPathChild = FileUtil::pathname(absPathParent, childFileName);
                ensurePrivate(absPathChild);

                struct ::stat statBuf;
                FileUtil::getStat(prodDirFd, absPathChild, statBuf, true); // Follow symlinks

                if (S_ISDIR(statBuf.st_mode)) {
                    scanRepo(absPathChild);
                }
                else {
                    Guard guard{mutex};
                    if (tryAddFile(absPathChild))
                        ++numAdded;
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
        procQueueCond.notify_all();
        deleteQueueCond.notify_all();
    }

    /**
     * Returns the product-name corresponding to the absolute pathname of a product-file.
     *
     * @param[in] pathname  Absolute pathname of a product-file
     * @return              Corresponding product-name
     */
    std::string getProdName(const std::string& pathname) const {
        LOG_ASSERT(FileUtil::isAbsolute(pathname));
        return pathname.substr(prodDirLen); // Remove `prodDir`+separator
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
        FileUtil::getStat(absPathname, statBuf, false);

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

private:
    ///< Thread on which products that are too old are deleted
    Thread deleteThread;

public:
    /**
     * Constructs.
     *
     * @param[in] prodPath         Pathname of root of directory hierarchy for complete product-
     *                             files
     * @param[in] maxOpenProds     Maximum number of products with open file descriptors
     * @param[in] keepTime         Number of seconds to keep products before deleting them
     * @param[in] lastProcTime     Modification-time of the last, successfully-process data-product
     * @throw     InvalidArgument  `maxOpenProds <= 0`
     * @throw     InvalidArgument  `keepTime <= 0`
     * @throw     SystemError      Couldn't open repository directory
     */
    Impl(   const String&      prodPath,
            const size_t       maxOpenProds,
            const int          keepTime,
            const SysTimePoint lastProcTime)
        : mutex()
        , stop(false)
        , procQueueCond()
        , deleteQueueCond()
        , keepTime(duration_cast<SysDuration>(seconds(keepTime)))
        , prodEntries(1000)
        , deleteQueue()
        , openProds()
        , maxOpenProds{maxOpenProds}
        , prodDir(FileUtil::ensureTrailingSep(FileUtil::makeAbsolute(prodPath)))
        , prodDirLen{prodDir.size()}
        , prodDirFd(::open(FileUtil::ensureDir(prodDir).data(), O_DIRECTORY | O_RDONLY))
        , threadEx()
        , procQueue()
        , lastProcessed(lastProcTime)
        , done(false)
        , deleteThread(&Impl::deleteProds, this)
    {
        if (maxOpenProds <= 0)
            throw INVALID_ARGUMENT("maxOpenProds=" + std::to_string(maxOpenProds));

        if (keepTime <= 0)
            throw INVALID_ARGUMENT("keepTime=" + std::to_string(keepTime));

        if (prodDirFd == -1)
            throw SYSTEM_ERROR("Couldn't open repository directory, \"" + prodDir + "\"");
    }

    virtual ~Impl() {
        {
            Guard guard{mutex};
            done = true;
            deleteQueueCond.notify_all();
        }
        deleteThread.join();

        if (prodDirFd >= 0)
            ::close(prodDirFd);
    }

    /**
     * Returns the absolute pathname of the root directory of the product-file hierarchy.
     * @return The absolute pathname of the root directory of the product-file hierarchy
     */
    String getRootDir() const noexcept
    {
        return prodDir;
    }

    /**
     * Returns the pathname of the top-level directory that contains only product-files.
     * @return The pathname of the top-level directory that contains only product-files
     */
    String getProdDir() const noexcept {
        return prodDir;
    }

    /**
     * Returns the next product to process, either for transmission or local processing. Blocks
     * until one is available. The returned product is active.
     * @return               The next product to process. Will test false if halt() has been called;
     *                       otherwise, the product's metadata and data will be complete.
     * @throws SystemError   System failure
     * @throws RuntimeError  inotify(7) failure
     * @see halt()
     */
    ProdEntry getNextProd() {
        ProdEntry prodEntry{};
        Lock      lock(mutex);

        for (;;) {
            procQueueCond.wait(lock, [&]{return !procQueue.empty() || stop || threadEx;});

            if (stop)
                break;

            threadEx.throwIfSet();

            auto procQueueIter = procQueue.begin();
            auto prodEntriesIter = getProdEntry(procQueueIter->second);
            procQueue.erase(procQueueIter);

            if (prodEntriesIter != prodEntries.end()) {
                prodEntry = *prodEntriesIter;
                openProds.erase(prodEntry.prodInfo.getId());
                break;
            }
        }

        return prodEntry;
    }

    /**
     * Causes getNextProd() to always return a ProdEntry that tests false.
     * @see getNextProd()
     */
    void halt() {
        Guard guard{mutex};
        stop = true;
        procQueueCond.notify_all();
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

Repository::Repository(Impl* impl) noexcept
    : pImpl(impl) {
}

Repository::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

String Repository::getRootDir() const noexcept {
    return pImpl->getRootDir();
}

String Repository::getProdDir() const noexcept {
    return pImpl->getProdDir();
}

ProdEntry Repository::getNextProd() const {
    return pImpl->getNextProd();
}

void Repository::halt() const {
    return pImpl->halt();
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
    Thread  watchThread; ///< Watcher thread

    /**
     * Executes the watcher of new product-files. Implemented as the start routine of a new thread.
     */
    void runWatcher() {
        try {
            //LOG_DEBUG("Starting thread to watch the repository");
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
     * Adds a directory to the repository's watch-list.
     *
     * @param[in] dirPath  Absolute pathname of a repository directory
     */
    void maybeWatch(const String& dirPath) {
        LOG_ASSERT(dirPath.find(prodDir) == 0);
        watcher.tryAdd(dirPath);
    }

public:
    /**
     * Constructs. Upon return, the repository has been completely scanned and all relevant
     * directories are being watched.
     * @param[in] prodPath      Pathname of the root-directory of the product-file hierarchy
     * @param[in] maxOpenFiles  Maximum number of files to have open simultaneously
     * @param[in] keepTime      Number of seconds to keep products before deleting them
     * @param[in] lastProcTime  Modification-time of the last, successfully-processed product-file
     */
    Impl(   const String&      prodPath,
            const long         maxOpenFiles,
            const int          keepTime,
            const SysTimePoint lastProcTime)
        : Repository::Impl{prodPath, static_cast<size_t>(maxOpenFiles), keepTime, lastProcTime}
        , watcher(prodDir, *this)
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
        scanRepo(prodDir);
    }

    ~Impl() {
        const auto nativeHandle = watchThread.native_handle();
        const auto threadId = std::to_string(nativeHandle);
        //LOG_DEBUG("Cancelling watch thread " + threadId);
        const auto status = ::pthread_cancel(nativeHandle);
        if (status)
            LOG_SYSERR("pthread_cancel() failure: %s", ::strerror(errno));
        //LOG_DEBUG("Joining watch thread " + threadId);
        watchThread.join();
        //LOG_DEBUG("Joined watch thread " + threadId);
    }

    /**
     * Processes the creation of a directory in the repository. Called by the Watcher.
     * @param[in] pathname  Absolute pathname of the directory
     */
    void dirAdded(const String& pathname) override {
        scanRepo(pathname);
    }

    /**
     * Processes the addition of a regular file to the repository. Called by the Watcher.
     * @param[in] pathname  Absolute pathname of the regular file
     */
    void fileAdded(const String& pathname) override {
        LOG_ASSERT(pathname.find(prodDir) == 0);

        Guard guard{mutex};
        tryAddFile(pathname);
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
        // A hard link results in an inappropriate modification-time on the repository's side
        softLink(extantPath, linkPath);
    }
};

/******************************************************************************/

PubRepo::PubRepo()
    : Repository{nullptr}
{}

PubRepo::PubRepo(
        const String&      rootPathname,
        const long         maxOpenFiles,
        const SysTimePoint lastProcTime,
        const int          keepTime)
    : Repository(new PubRepo::Impl{rootPathname, maxOpenFiles, keepTime, lastProcTime})
{}

void PubRepo::link(
            const String& pathname,
            const String& prodName) const {
    static_cast<PubRepo::Impl*>(pImpl.get())->link(pathname, prodName);
}

/******************************************************************************/
/******************************************************************************/

/**
 * Implementation of a subscriber's repository.
 */
class SubRepo::Impl final : public Repository::Impl
{
    static const String INCOMPLETE_DIR_NAME;
    static const String COMPLETE_DIR_NAME;

    /// Pathname of the directory for holding incomplete product files
    const String       incompleteDir;
    LastProdPtr        lastReceived; ///< Object for identifying the most recently-received product
    const bool         queueProds;   ///< Queue complete data-products for processing?
    unsigned long      totalProds;   ///< Total number of products.
    unsigned long long totalBytes;   ///< Total number of bytes of all products.
    unsigned long long totalLatency; ///< Total latency of all products. A uint64_t should be good
                                     ///< for about 584 years of a nanosecond system clock

    /**
     * Returns a temporary absolute pathname to hold an incomplete data-product.
     *
     * @param[in] prodId  Product identifier
     * @return            Temporary absolute pathname
     */
    inline String tempAbsPathname(const ProdId prodId) {
        return FileUtil::pathname(incompleteDir, prodId.to_string());
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
            deleteQueueCond.notify_all(); // State changed
        }

        return *iter;
    }

    /**
     * Handles an arrived data-product if it's now complete. The product is closed and its metadata
     * is added to the processing-queue for local processing.
     *
     * @pre                  State is locked
     * @param[in] prodEntry  Product entry
     * @post                 State is locked
     */
    void tryFinish(const ProdEntry& prodEntry) {
        LOG_ASSERT(!mutex.try_lock());

        if (prodEntry.prodInfo) {
            auto& prodFile = prodEntry.prodFile;
            if (prodFile.isComplete()) {
                const auto& prodInfo = prodEntry.prodInfo;
                const auto  prodId = prodInfo.getId();
                const auto& createTime = prodInfo.getCreateTime();
                const auto  sysClockLatency = (SysClock::now() - createTime).count();

                ++totalProds;
                totalBytes += prodInfo.getSize();
                totalLatency += sysClockLatency;

                // The creation-time of the product is saved in the modification-time of the file
                prodFile.setModTime(createTime);

                LOG_INFO("Received " + prodInfo.to_string() + ". Latency=" +
                        std::to_string(sysClockLatency * sysClockRatio) + " s");

                const auto newPathname = prodDir + prodInfo.getName();
                FileUtil::ensureDir(FileUtil::dirname(newPathname), 0755);
                prodFile.rename(newPathname);
                prodFile.close(); // Removal from processing-queue will re-enable access

                const auto modTime = prodFile.getModTime();
                lastReceived->save(modTime);

                openProds.erase(prodId);
                if (queueProds) {
                    /*
                     * The file's modification-time is used rather than the product's creation-time
                     * because the two might have different resolutions and the modification-time
                     * will be used to locally process files that arrived since the last
                     * successfully-processed product from the previous session.
                     */
                    procQueue.emplace(modTime, prodInfo.getId());
                    procQueueCond.notify_all();
                }
            }
        }
    }

public:
    Impl(   const std::string& rootDir,
            const size_t       maxOpenFiles,
            const int          keepTime,
            const LastProdPtr& lastReceived,
            const bool         queueProds)
        : Repository::Impl{FileUtil::pathname(rootDir, COMPLETE_DIR_NAME), maxOpenFiles, keepTime,
                lastReceived->recall()}
        , incompleteDir(FileUtil::pathname(FileUtil::ensureTrailingSep(
                FileUtil::makeAbsolute(rootDir)), INCOMPLETE_DIR_NAME))
        , lastReceived(lastReceived)
        , queueProds(queueProds)
        , totalProds(0)
        , totalBytes(0)
        , totalLatency(0)
    {
        /**
         * Ignore incomplete data-products and start from scratch. They will be completely
         * recovered as part of the backlog.
         */
        FileUtil::rmDirTree(incompleteDir);
        scanRepo(prodDir); // NB: No "incomplete" directory for it to scan
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
                tryFinish(*iter);
                wasSaved = true;
            }
        }
        else {
            String           pathname = tempAbsPathname(prodId);
            ProdFile         prodFile(pathname, prodInfo.getSize());
            const ProdEntry& prodEntry = add(ProdEntry{prodInfo, prodFile});

            tryFinish(prodEntry); // Supports products with no data
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
                tryFinish(*iter);
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

    /**
     * Returns the total number of products received.
     * @return  The total number of products received
     */
    long getTotalProds() const noexcept {
        return totalProds;
    }

    /**
     * Returns the sum of the size of all products in bytes.
     * @return The sum of the size of all products in bytes
     */
    long long getTotalBytes() const noexcept {
        return totalBytes;
    }

    /**
     * Returns the sum of the latencies of all products in seconds.
     * @return The sum of the latencies of all products in seconds
     */
    double getTotalLatency() const noexcept {
        return totalLatency * sysClockRatio;
    }
};

const String SubRepo::Impl::INCOMPLETE_DIR_NAME = "incomplete";
const String SubRepo::Impl::COMPLETE_DIR_NAME = "products";

/**************************************************************************************************/

SubRepo::SubRepo()
    : Repository{nullptr}
{}

SubRepo::SubRepo(
        const std::string& rootPathname,
        const size_t       maxOpenFiles,
        const LastProdPtr& lastReceived,
        const bool         queueProds,
        const int          keepTime)
    : Repository(new Impl{rootPathname, maxOpenFiles, keepTime, lastReceived, queueProds})
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

long SubRepo::getTotalProds() const noexcept {
    return static_cast<Impl*>(pImpl.get())->getTotalProds();
}

long long SubRepo::getTotalBytes() const noexcept {
    return static_cast<Impl*>(pImpl.get())->getTotalBytes();
}

double SubRepo::getTotalLatency() const noexcept {
    return static_cast<Impl*>(pImpl.get())->getTotalLatency();
}

} // namespace
