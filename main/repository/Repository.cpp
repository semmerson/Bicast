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

using Duration  = SysClock::duration;

static String INCOMPLETE_DIR_NAME(".incomplete");

/**
 * @tparam PF  Product-file type
 */
struct ProdEntry final
{
    ProdFile  prodFile;  ///< Product file
    ProdInfo  prodInfo;  ///< Product information

    ProdEntry()
        : prodFile{}
        , prodInfo()
    {}

    explicit ProdEntry(
            ProdFile       prodFile,
            const ProdInfo prodInfo = ProdInfo{})
        : prodFile(prodFile)
        , prodInfo(prodInfo)
    {}

    explicit ProdEntry(
            ProdFile&&       prodFile,
            const ProdInfo&& prodInfo = ProdInfo{})
        : prodFile(prodFile)
        , prodInfo(prodInfo)
    {}

    explicit ProdEntry(const String& pathname)
        : ProdEntry(ProdFile(pathname))
    {
        LOG_ASSERT(!FileUtil::isAbsolute(pathname));
    }

    ~ProdEntry() noexcept
    {}

    inline ProdEntry& operator=(const ProdEntry& rhs) =default;

    /**
     * @return  Product metadata. Will test false if it hasn't been set by `set(const ProdInfo)`.
     * @see `set(const ProdInfo)`
     */
    inline ProdInfo getProdInfo() const {
        return ProdInfo{};
    }

    inline std::string to_string() const
    {
        return prodFile.to_string();
    }
};

/**************************************************************************************************/

/**
 * Abstract, base implementation of a repository of transient data-products.
 */
class Repository::Impl
{
    struct DeleteEntry
    {
        SysTimePoint deleteTime;
        ProdId       prodId;

        DeleteEntry(
                const SysTimePoint& deleteTime,
                const ProdId        prodId)
            : deleteTime(deleteTime)
            , prodId(prodId)
        {}

        bool operator<(const DeleteEntry& rhs) {
            // `>` because `deleteQueue.top()` returns maximum entry
            return deleteTime > rhs.deleteTime;
        }
    };

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

        if (prodEntry)
            ensureOpen(prodId, *prodEntry);

        return prodEntry;
    }

    void scour() {
        try {
            for (;;)
                productDb.getNextDelete().deleteFile();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
            threadEx.set(ex);
            stopExecution();
        }
    }

    using ScanMap = std::map<Timestamp, ProdEntry>;

    /**
     * Scans the repository. Adds products to the database.
     *
     * @param[in] absPathParent  Absolute pathname of directory to recursively scan
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

                if (::strcmp(".", childFileName) == 0 || ::strcmp("..", childFileName) == 0 ||
                        ::strcmp(INCOMPLETE_DIR_NAME.data(), childFileName) == 0)
                    continue;

                const std::string absPathChild = absPathParent + "/" + childFileName;
                const std::string relPathChild = absPathChild.substr(rootPrefixLen);
                struct stat       statBuf;

                FileUtil::lstat(absPathChild, statBuf);

                if (S_ISDIR(statBuf.st_mode)) {
                    scanRepo(absPathChild);
                }
                else if (S_ISLNK(statBuf.st_mode)) {
                    throw RUNTIME_ERROR("Repository doesn't yet support symbolic links like \"" +
                            absPathChild + "\"");
                }
                else if (S_ISREG(statBuf.st_mode) && productDb.add(relPathChild)) {
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

    /**
     * Ensures that an existing product-file is open and puts it at the back of the open-products
     * queue.
     *
     * @param[in] prodId       Product identifier
     * @throw OutOfRangeError  No such product-file
     */
    ProdFile activate(const ProdId prodId) {
        LOG_ASSERT(!mutex.try_lock());
        auto& prodFile = prodEntries.at(prodId).prodFile;
        prodFile.open(); // Idempotent
        openProds.erase(prodId); // Idempotent
        openProds.push(prodId); // Idempotent
        return prodFile;
    }

    void stopExecution() {
        Guard guard{mutex};
        stop = true;
        cond.notify_all();
    }

    /**
     * Adds a product from an existing file.
     *
     * @param[in] pathname  Pathname of file relative to root directory
     * @retval    `true`    Referenced product was added
     * @retval    `false`   Referenced product was not added because it's a duplicate
     */
    bool add(const String& pathname) {
        LOG_ASSERT(!FileUtil::isAbsolute(pathname));

        Guard     guard{mutex};
        ProdId    prodId{pathname};
        ProdEntry prodEntry{pathname};
        bool      wasAdded = prodEntries.emplace(prodId, prodEntry).second;

        if (wasAdded) {
            auto& prodFile = prodEntry.prodFile;
            prodFile.close(); // No need for it to be open at this time
            SysTimePoint modTime;;
            deleteQueue.push(DeleteEntry{prodFile.getModTime(modTime)+keepTime, prodId});
            cond.notify_all(); // Database state changed
        }

        return wasAdded;
    }

    ProdFile getNextDelete() {
        Lock lock{mutex};
        cond.wait(lock, [&] {return !deleteQueue.empty() &&
                deleteQueue.top().deleteTime >= SysClock::now();});

        const auto prodId = deleteQueue.top().prodId;
        auto&      prodFile = prodEntries.at(prodId).prodFile;

        prodFile.close();
        FileUtil::removeFileAndPrune(rootFd, prodFile.getPathname());
        openProds.erase(prodId);
        prodEntries.erase(prodId);
        deleteQueue.pop();

        return prodFile;
    }

    void scour() {
        /**
         * The following code deletes products on-time regardless of their arrival.
         */
        static const auto endOfTime = SysTimePoint() + duration_values<SysTimePoint::rep>::max();
        Lock              lock{mutex};

        for (auto deleteTime{deleteQueue.empty() ? endOfTime : deleteQueue.top().deleteTime}; ;
                deleteTime = deleteQueue.empty() ? endOfTime : deleteQueue.top().deleteTime) {
            if (cond.wait_until(lock, deleteTime, [&] {return !deleteQueue.empty() &&
                    deleteQueue.top().deleteTime >= SysClock::now();})) {
                const auto prodId = deleteQueue.top().prodId;
                auto       prodFile = prodEntries.at(prodId).prodFile;

                prodFile.close();
                FileUtil::removeFileAndPrune(rootFd, prodFile.getPathname());
                openProds.erase(prodId);
                prodEntries.erase(prodId);
                deleteQueue.pop();
            }
        }
    }

    ProdFile findProdFile(const ProdId prodId) {
        static ProdFile invalid{};
        Guard guard{mutex};
        return (prodEntries.count(prodId))
                ? activate(prodId)
                : invalid;
    }

    ProdEntry* findProdEntry(const ProdId prodId) {
        LOG_ASSERT(!mutex.try_lock());

        if (prodEntries.count(prodId) == 0)
            return nullptr;

        auto prodEntry = &prodEntries.at(prodId);
        prodEntry->prodFile.open(); // Idempotent
        openProds.erase(prodId);
        openProds.push(prodId);

        return prodEntry;
    }

    ProdEntry getProdEntry(
            const ProdId   prodId,
            const String&  pathname,
            const ProdSize prodSize) {
        Guard guard{mutex};
        return prodEntries.count(prodId)
                ? prodEntries.at(prodId)
                : prodEntries.emplace(prodId, ProdEntry{
                        INCOMPLETE_DIR_NAME + "/" + prodId.to_string()}).first->second;
    }

protected:
    using ProdEntries = std::unordered_map<ProdId, ProdEntry>;
    using DeleteQueue = std::priority_queue<DeleteEntry>;
    using Pair        = std::pair<ProdEntry&, bool>;

    mutable Mutex        mutex;          ///< To maintain consistency
    mutable Cond         cond;           ///< For inter-thread communication
    const Duration       keepTime;       ///< Length of time to keep products
    ProdEntries          prodEntries;    ///< Product entries
    DeleteQueue          deleteQueue;    ///< Queue of when to delete products
    HashSetQueue<ProdId> openProds;      ///< Products whose files are open
    const size_t         maxOpenProds;   ///< Maximum number of open products
    bool                 stop;           ///< Stop execution because scour thread terminated?
    const String         absPathRoot;    ///< Absolute pathname of root directory of repository
    size_t               rootPrefixLen;  ///< Length in bytes of root pathname prefix
    int                  rootFd;         ///< File descriptor open on root-directory of repository
    size_t               maxOpenFiles;   ///< Max number open files
    ThreadEx             threadEx;       ///< Subtask exception
    const Duration       keepTime;       ///< Duration to keep products
    Thread               scourThread;    ///< Thread for deleting old products
    std::queue<ProdInfo> prodQueue;      ///< Queue of products for external processing


    void ensureOpen(
            const ProdId prodId,
            ProdEntry&   prodEntry) {
        LOG_ASSERT(!mutex.try_lock());

        openProds.erase(prodId);
        while (openProds.size() >= maxOpenFiles) {
            allProds.get(openProds.front())->close();
            openProds.pop();
        }
        prodEntry.open(); // Idempotent
        openProds.push(prodId);
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

    void addToOpenList(const ProdId prodId) {
        LOG_ASSERT(!mutex.try_lock());

        openProds.push(prodId);

        while (openProds.size() > maxOpenProds) {
            prodEntries.at(openProds.front()).prodFile.close();
            openProds.pop();
        }
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
        : mutex()
        , cond()
        , keepTime(duration_cast<Duration>(seconds(keepTime)))
        , prodEntries()
        , deleteQueue()
        , openProds()
        , maxOpenProds{maxOpenProds}
        , stop(false)
        , absPathRoot(FileUtil::makeAbsolute(rootDir))
        , rootPrefixLen{absPathRoot.size() + 1}
        , rootFd(::open(FileUtil::ensureDir(absPathRoot).data(), O_DIRECTORY | O_RDONLY))
        , threadEx()
        , scourThread()
        , prodQueue()
    {
        if (maxOpenProds <= 0)
            throw INVALID_ARGUMENT("maxOpenProds=" + std::to_string(maxOpenProds));

        if (keepTime <= Duration(0))
            throw INVALID_ARGUMENT("keepTime=" + std::to_string(keepTime.count()));

        if (rootFd == -1)
            throw SYSTEM_ERROR("Couldn't open root-directory of repository, \"" + absPathRoot +
                    "\"");

        try {
            scourThread = Thread(&Impl::scour, this);

            scanRepo(absPathRoot);
        } // `rootFd` is open
        catch (const std::exception& ex) {
            ::close(rootFd);
            throw;
        }
    }

    virtual ~Impl() noexcept {
        stopExecution();
        scourThread.join();

        if (rootFd >= 0)
            ::close(rootFd);
    }

    const std::string& getRootDir() const noexcept
    {
        return absPathRoot;
    }

    virtual ProdInfo getNextProd() =0;

    ProdInfo getProdInfo(const ProdId prodId) {
        threadEx.throwIfSet();

        static ProdInfo empty{};
        Guard           guard{mutex};

        return (prodEntries.count(prodId))
                ? prodEntries.at(prodId).prodInfo
                : empty; // Will test false
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

        if (prodEntries.count(segId.prodId) == 0)
            return DataSeg{};

        auto& prodFile = prodEntries.at(segId.prodId).prodFile;
        return DataSeg{segId, prodFile.getFileSize(), prodFile.getData(segId.offset)};
    }

    /**
     * Returns the absolute pathname of the file corresponding to a product name.
     *
     * @pre                 `name.size() > 0`
     * @param[in] prodName  Product name
     * @return              Absolute pathname of corresponding file
     */
    std::string getAbsPathname(const std::string& name) const
    {
        LOG_ASSERT(name.size());
        return absPathRoot + "/" + name;
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

ProdInfo Repository::getNextProd() const {
    return pImpl->getNextProd();
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

        ProdEntry entry{prodFile};
        if (allProds.push(prodId, entry)) {
            prodQueue.push(prodId);
            cond.notify_all();
        }
    }

public:
    Impl(   const String& rootPathname,
#ifdef OPEN_MAX
            const long    maxOpenFiles = OPEN_MAX/2)
#else
            const long    maxOpenFiles = sysconf(_SC_OPEN_MAX)/2,
#endif
            const int     keepTime)
        : Repository::Impl{rootPathname, static_cast<size_t>(maxOpenFiles), keepTime}
        , watcher(absPathRoot)
    {}

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

    /**
     * Returns information on the next product to publish. Blocks until one is ready. Watches the
     * repository's directory hierarchy for new files. A product-entry is created and added to the
     * set of active product-entries. for each new non-directory file in the repository's hierarchy:
     *
     * @return              Information on the next product to publish
     * @throws SystemError  System failure
     * @threadsafety        Compatible but unsafe
     */
    ProdInfo getNextProd() override {
        threadEx.throwIfSet();

        ProdInfo prodInfo{};
        String   prodName;

        try {
            Watcher::WatchEvent event;
            watcher.getEvent(event);

            //LOG_DEBUG("event.pathname=%s", event.pathname.data());
            //LOG_DEBUG("rootPrefixLen=%zu", rootPrefixLen);
            prodName = event.pathname.substr(rootPrefixLen);
            ProdFile prodFile(absPathRoot + '/' + prodName);

            const ProdId prodId(prodName);
            prodInfo = ProdInfo(prodId, prodName, prodFile.getFileSize());

            addProd(prodId, prodFile);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create product-file for \"" + prodName +
                    "\""));
        }

        return prodInfo;
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

void PubRepo::link(
        const std::string& pathname,
        const std::string& prodName) {
    return static_cast<Impl*>(pImpl.get())->link(pathname, prodName);
}

/******************************************************************************/
/******************************************************************************/

/**
 * Implementation of a subscriber's repository.
 */
class SubRepo::Impl final : public Repository::Impl
{
    /**
     * Returns a temporary absolute pathname.
     *
     * @param[in] prodId  Product identifier
     * @return            Temporary absolute pathname
     */
    inline String tempAbsPathname(const ProdId prodId) {
        return absPathRoot + '/' + INCOMPLETE_DIR_NAME + '/' + prodId.to_string();
    }

    /**
     * Adds a product-entry. Upon return, the entry is open, at the back of the open-products list,
     * and in the delete-queue.
     *
     * @param[in] prodId     Product-ID
     * @param[in] prodEntry  Product-entry
     * @return               Pointer to the added entry
     */
    ProdEntry* add(
            const ProdId prodId,
            ProdEntry&&  prodEntry) {
        LOG_ASSERT(!mutex.try_lock());

        auto pair = prodEntries.emplace(prodId, prodEntry);
        auto entryPtr = &*pair.first;

        if (pair.second) {
            deleteQueue.push(DeleteEntry{prodEntry.prodInfo.getCreateTime()+keepTime, prodId});
            prodEntry.prodFile.open();
            openProds.erase(prodId);
            addToOpenList(prodId);
            cond.notify_all(); // State changed
        }

        return entryPtr;
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

        auto prodEntry = findProdEntry(prodId);

        if (prodEntry == nullptr) {
            ProdFile  prodFile{tempAbsPathname(prodId), prodSize};
            prodEntry = add(prodId, ProdEntry{prodFile});
        }

        return *prodEntry;
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
    {}

    /**
     * Saves product-information. Creates a new product if necessary. If the resulting product
     * becomes complete, then it will be eventually returned by `getNextProd()`.
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
        threadEx.throwIfSet();

        bool       wasSaved = false;
        const auto prodId = prodInfo.getId();
        Guard      guard{mutex};
        auto       prodEntry = findProdEntry(prodId);

        if (prodEntry) {
            // Entry is open and appended to open-list
            if (!prodEntry->prodInfo) {
                prodEntry->prodInfo = prodInfo;
                finishIfComplete(prodId, *prodEntry);
                wasSaved = true;
            }
        }
        else {
            ProdFile  prodFile{tempAbsPathname(prodId), prodInfo.getProdSize()}; // Is open
            prodEntry = add(prodId, ProdEntry{prodFile, prodInfo});
            finishIfComplete(prodId, *prodEntry); // Supports products with no data
            wasSaved = true;
        }

        return wasSaved;
    }

    /**
     * Saves a data-segment. If the resulting product becomes complete, then it will be eventually
     * returned by `getNextProd()`.
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
        threadEx.throwIfSet();

        bool       wasSaved = false;
        const auto prodId = dataSeg.getId().prodId;
        auto       prodEntry = findProdEntry(prodId);

        if (prodEntry) {
            if (prodEntry->prodFile.save(dataSeg)) {
                finishIfComplete(prodId, *prodEntry);
                wasSaved = true;
            }
        }
        else {
            ProdFile prodFile{tempAbsPathname(prodId), dataSeg.getProdSize()}; // Is open
            prodFile.save(dataSeg);
            prodEntry = add(prodId, ProdEntry{prodFile});
            finishIfComplete(prodId, *prodEntry); // Supports products with no data
            wasSaved = true;
        }

        return wasSaved;
    }

    /**
     * Returns information on the next, completed data-product. Blocks until one is available.
     *
     * @return Next, completed data-product
     */
    ProdInfo getNextProd() override {
        Lock lock(mutex);

        cond.wait(lock, [&]{return !prodQueue.empty() || stop;});

        threadEx.throwIfSet();

        auto prodInfo = prodQueue.front();
        prodQueue.pop();

        return prodInfo;
    }

    /**
     * Indicates if a data-product's metadata exists.
     *
     * @param[in] prodId     Product identifier
     * @retval    `true`     Product metadata does exist
     * @retval    `false`    Product metadata does not exist
     */
    bool exists(const ProdId prodId)
    {
        threadEx.throwIfSet();

        Guard guard{mutex};
        auto  prodEntry = findProdEntry(prodId);
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
        threadEx.throwIfSet();

        Guard guard{mutex};
        auto  prodEntry = findProdEntry(segId.prodId);
        return prodEntry && prodEntry->prodFile.exists(segId.offset);
    }
};

/**************************************************************************************************/

SubRepo::SubRepo()
    : Repository{nullptr}
{}

SubRepo::SubRepo(
        const std::string& rootPathname,
        const size_t       maxOpenFiles,
        const int          keepTime)
    : Repository{new Impl(rootPathname, maxOpenFiles, keepTime)} {
}

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
