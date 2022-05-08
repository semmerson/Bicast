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
template <class PF>
class ProdEntry
{
public:
    ProdInfo  prodInfo;  ///< Product information
    PF        prodFile;  ///< Product file
    time_t    when;      ///< Creation-time of entry
    ProdId prev;      ///< Previous entry in queue
    ProdId next;      ///< Next entry in queue

    ProdEntry()
        : prodFile{}
        , prodInfo{}
        , when{time(nullptr)}
        , prev{}
        , next{}
    {}

    ProdEntry(
            const ProdInfo&    prodInfo,
            PF&                prodFile)
        : prodInfo(prodInfo)
        , prodFile(prodFile)
        , when{time(nullptr)}
        , prev{}
        , next{}
    {}

    virtual ~ProdEntry() noexcept
    {}

    inline ProdEntry& operator =(const ProdEntry& rhs) =default;

    inline const ProdInfo& getProdInfo() const
    {
        return prodInfo;
    }

    inline const std::string& getProdName() const
    {
        return prodInfo.getName();
    }

    inline PF& getProdFile()
    {
        return prodFile;
    }

    inline bool exists(const ProdSize offset) const
    {
        return prodFile.exists(offset);
    }

    std::string to_string() const
    {
        return prodInfo.to_string();
    }

    //virtual bool isComplete() const =0;
};

class SndProdEntry final : public ProdEntry<SndProdFile>
{
public:
    SndProdEntry()
        : ProdEntry()
    {}

    SndProdEntry(
            const std::string& prodName,
            const ProdId       prodId,
            SndProdFile        prodFile)
        : ProdEntry(ProdInfo(prodId, prodName, prodFile.getProdSize()),
                prodFile)
    {}

    bool isComplete() const
    {
        return true;
    }
};

/**************************************************************************************************/

/**
 * Abstract, base implementation of a repository of temporary data-products.
 */
class Repository::Impl
{
protected:
    /**
     * A product-index to product-file hash table whose entries form a doubly-linked list.
     *
     * @tparam PF  Type of product-file
     */
    template<class PF>
    class LinkedProdMap final
    {
        /*
         * Alternative implementations include using either an std::list<PF> or an
         * std::map<ProdIndex, PF> in conjunction with an std::unordered map<ProdIndex, PF>. Neither
         * would scale as efficiently as incorporating the doubly-linked list into an unordered map.
         */

        /**
         * A hash table entry for a product-file that forms a linked-list.
         */
        struct Entry final
        {
            PF          prodFile; ///< Receiving product-file
            ProdId   prev;     ///< Previous entry (towards the head)
            ProdId   next;     ///< Subsequent entry (towards the tail)

            Entry()
                : prodFile()
                , prev()
                , next()
            {}

            Entry(PF& prodFile, const ProdId prev)
                : prodFile(prodFile)
                , prev(prev)
                , next()
            {}
        };

        std::unordered_map<ProdId, Entry> map;
        ProdId                            head; ///< Head of list
        ProdId                            tail; ///< Tail of list

    public:
        /**
         * Default constructs.
         */
        LinkedProdMap()
            : map()
            , head()
            , tail()
        {}

        /**
         * Constructs with an initial number of buckets for the map.
         *
         * @param[in] initSize  Initial size
         */
        LinkedProdMap(const size_t initSize)
            : map(initSize)
            , head()
            , tail()
        {}

        /**
         * Returns the number of entries.
         *
         * @return Number of entries
         */
        size_t size() {
            return map.size();
        }

        /**
         * Adds an entry. The entry will be at the tail-end of the list.
         *
         * @param[in] prodId      Product identifier
         * @param[in] prodFile    Associated product-file
         * @throws    LogicError  An entry already exists for the product-index
         */
        void pushBack(
                const ProdId prodId,
                PF           prodFile)
        {
            const bool wasEmpty = map.empty();
            auto       pair = map.insert({prodId, Entry(prodFile, tail)});

            if (!pair.second)
                throw LOGIC_ERROR("Entry already exists");

            if (wasEmpty) {
                head = prodId;
            }
            else {
                map[tail].next = prodId;
            }
            tail = prodId;
        }

        /**
         * Removes an entry.
         *
         * @param[in] prodId        Product identifier of entry to be removed
         * @return                  Product-file associated with product-index
         * @throws InvalidArgument  No such entry
         */
        PF erase(const ProdId prodId)
        {
            auto iter = map.find(prodId);

            if (iter == map.end())
                throw INVALID_ARGUMENT("An entry for product " + prodId.to_string() +
                        " doesn't exist");

            Entry& entry = iter->second;

            if (head == prodId) {
                head = entry.next;
            }
            else {
                map[entry.prev].next = entry.next;
            }

            if (tail == prodId) {
                tail = entry.prev;
            }
            else {
                map[entry.next].prev = entry.prev;
            }

            auto prodFile = entry.prodFile;;
            map.erase(iter);
            return prodFile;
        }

        /**
         * Returns the product-file associated with a product-index if it exists. The file will be
         * moved to the end of the list.
         *
         * @param[in] prodId     Product identifier
         * @return               Associated product-file. Will test false if it doesn't exist.
         */
        PF find(const ProdId prodId)
        {
            auto iter = map.find(prodId);

            if (iter == map.end()) {
                static const PF prodFile{};
                return prodFile;
            }

            auto prodFile = erase(prodId);
            pushBack(prodId, prodFile);
            return prodFile;
        }

        /**
         * Removes and returns the product-file at the head of the list (i.e., the least
         * recently-used).
         *
         * @return             Least recently-used product-file
         * @throw  OutOfRange  The list is empty
         */
        PF popHead()
        {
            if (map.empty())
                throw OUT_OF_RANGE("The list is empty");
            return erase(head);
        }
    };

    mutable Mutex     mutex;        ///< For concurrency
    mutable Cond      cond;         ///< For concurrency
    const std::string rootPathname; ///< Absolute pathname of root directory of repository
    size_t            rootPrefixLen;///< Length in bytes of root pathname prefix
    int               rootFd;       ///< File descriptor open on root-directory of repository
    const SegSize     segSize;      ///< Size of canonical data-segment in bytes
    size_t            maxOpenFiles; ///< Max number open files

    Impl(   const std::string& rootPathname,
            const SegSize      segSize,
            const long         maxOpenFiles)
        : mutex{}
        , cond()
        , rootPathname(makeAbsolute(rootPathname))
        , rootPrefixLen{rootPathname.size() + 1}
        , rootFd(-1)
        , segSize{segSize}
        , maxOpenFiles{static_cast<size_t>(maxOpenFiles)}
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

    /**
     * Ensures that there's one less than the maximum number of open product-files.
     *
     * @pre State is locked
     */
    template<class PF>
    static void ensureRoom(
            LinkedProdMap<PF>& map,
            const size_t       maxOpenFiles) {
        while (map.size() >= maxOpenFiles)
            map.popHead().close();
    }

    static std::string getIndexPath(const ProdId prodId)
    {
        auto  index = (ProdId::Type)prodId;
        char  buf[sizeof(index)*3 + 1 + 1]; // Room for final '/'
        char* cp = buf;

        for (int nshift = 8*(sizeof(index)-1); nshift >= 0; nshift -= 8) {
            (void)sprintf(cp, "%.2x/", (index >> nshift) & 0xff);
            cp += 3;
        }
        *--cp = 0; // Squash final '/'

        return buf;
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
            auto iter = prodFiles.find(prodId);
            if (iter == prodFiles.end())
                break;
            Entry& prodEntry = iter.second;
            next = prodEntry.next;
            if (time(nullptr) - prodEntry.when <= 86400)
                break;
            prodFiles.erase(iter);
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
    Repository::Impl::LinkedProdMap<SndProdFile> prodFiles; ///< All existing product-files
    Repository::Impl::LinkedProdMap<SndProdFile> openFiles; ///< All open product-files
    Watcher                                      watcher;   ///< Watches filename hierarchy
    std::queue<ProdId>                           prodQueue; ///< Queue of products to be sent
    ProdId                                       prodId;    ///< Next product-identifier

    ProdId getNextId()
    {
        // TODO: Make persistent between sessions
        return ++prodId;
    }

    /**
     * Ensures that there's one less than the maximum number of open product- files.
     *
     * @pre State is locked
    void ensureRoom() {
        while (openFiles.size() >= maxOpenFiles) {
            const auto prodId = openFiles.getHead();
            openFiles.find(prodId).close();
            openFiles.remove(prodId);
        }
    }
     */

    /**
     * Adds a new product-file.
     *
     * @pre                  State is unlocked
     * @param[in] prodId     Product-identifier
     * @param[in] prodFile   Product-file
     */
    void addProdFile(
            const ProdId prodId,
            SndProdFile  prodFile) {
        Guard guard(mutex);

        prodFiles.pushBack(prodId, prodFile);
        ensureRoom<SndProdFile>(openFiles, maxOpenFiles);
        openFiles.pushBack(prodId, prodFile);
        prodQueue.push(prodId);
        cond.notify_all();
    }

    /**
     * Returns the product-file corresponding to a product-index. The product- file is open and at
     * the tail-end of the open-files list.
     *
     * @pre                  State is locked
     * @param[in] prodId     Product identifier of product-entry
     * @return               Corresponding product-file. Will test false if it
     *                       doesn't exist.
     */
    SndProdFile getProdFile(const ProdId prodId)
    {
        LOG_ASSERT(!mutex.try_lock());

        auto prodFile = openFiles.find(prodId);

        if (!prodFile) {
            prodFile = prodFiles.find(prodId);

            if (prodFile) {
                prodFile.open(rootFd);
                ensureRoom<SndProdFile>(openFiles, maxOpenFiles);
                openFiles.pushBack(prodId, prodFile);
            }
        }

        return prodFile;
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize,
            const long         maxOpenFiles = sysconf(_SC_OPEN_MAX)/2)
        : Repository::Impl{rootPathname, segSize, maxOpenFiles}
        , prodFiles()
        , watcher(this->rootPathname) // Is absolute pathname
        , prodQueue()
        , prodId()
    {
        if (maxOpenFiles <= 0)
            throw INVALID_ARGUMENT("maxOpenFiles=" + std::to_string(maxOpenFiles));
        prodFiles = LinkedProdMap<SndProdFile>(maxOpenFiles);
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
     * @return              Index of next product to publish
     * @throws SystemError  System failure
     * @threadsafety        Compatible but unsafe
     */
    ProdInfo getNextProd()
    {
        ProdInfo   prodInfo{};
        const auto prodId = getNextId();

        try {
            Watcher::WatchEvent event;
            watcher.getEvent(event);

            const auto prodName = event.pathname.substr(rootPrefixLen);

            SndProdFile prodFile(rootFd, prodName, segSize);
            addProdFile(prodId, prodFile);

            prodInfo = ProdInfo(prodId, prodName, prodFile.getProdSize());
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't get product-file corresponding to "
                    "product ID " + prodId.to_string()));
        }

        return prodInfo;
    }

    /**
     * Returns the product-information corresponding to a product-index.
     *
     * @param[in] prodId     Product identifier
     * @return               Corresponding product-information. Will test false if no such
     *                       information exists.
     */
    ProdInfo getProdInfo(const ProdId prodId)
    {
        Guard      guard{mutex};
        const auto prodFile = getProdFile(prodId);

        if (!prodFile) {
            static const ProdInfo prodInfo{};
            return prodInfo;
        }

        return ProdInfo(prodId, prodFile.getPathname(),
                prodFile.getProdSize());
    }

    /**
     * Returns the in-memory data-segment that corresponds to a segment identifier.
     *
     * @param[in] segId  Data-segment identifier
     * @return           Corresponding in-memory data-segment Will test false if no such segment
     *                   exists.
     * @see `DataSeg::operator bool()`
     */
    DataSeg getDataSeg(const DataSegId segId)
    {
        Guard      guard{mutex};
        const auto prodFile = getProdFile(segId.prodId);

        if (!prodFile) {
            static const DataSeg dataSeg{};
            return dataSeg;
        }

        const auto offset = segId.offset;

        return DataSeg(segId, prodFile.getProdSize(),
                prodFile.getData(segId.offset));
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
    std::queue<ProdInfo>       completeProds; ///< Queue of completed products
    Repository::Impl::LinkedProdMap<RcvProdFile> prodFiles;     ///< Known product-files
    Repository::Impl::LinkedProdMap<RcvProdFile> openFiles;     ///< Known and open product-files

    void makeRoom()
    {
        while (openFiles.size() >= maxOpenFiles)
            openFiles.popHead().close();
    }

    /**
     * Returns the product-file corresponding to a product-index if it exists. The file is
     *   - Open
     *   - At the tail-end of the open-files list
     *
     * @pre                  State is locked
     * @param[in] prodId     Product identifier
     * @return               Product-file corresponding to product-index. Will test false if it
     *                       doesn't exist.
     */
    RcvProdFile getProdFile(const ProdId prodId)
    {
        LOG_ASSERT(!mutex.try_lock());

        RcvProdFile prodFile = openFiles.find(prodId);

        if (!prodFile) {
            prodFile = prodFiles.find(prodId);

            if (prodFile) {
                ensureRoom<RcvProdFile>(openFiles, maxOpenFiles);
                prodFile.open(rootFd);
                openFiles.pushBack(prodId, prodFile);
            }
        }

        return prodFile;
    }

    /**
     * Adds a new product-file.
     *
     * @pre                  State is locked
     * @param[in] prodId     Product identifier
     * @param[in] prodFile   Product-file
     */
    void addProdFile(
            const ProdId prodId,
            RcvProdFile  prodFile) {
        LOG_ASSERT(!mutex.try_lock());

        prodFiles.pushBack(prodId, prodFile);
        openFiles.pushBack(prodId, prodFile);
    }

    /**
     * Returns the product-file that corresponds to a product-index. The file is
     *   - Created if necessary
     *   - Open
     *   - At the tail-end of the open-files list
     *
     * @pre                  State is unlocked
     * @param[in] prodId     Product identifier
     * @param[in] prodSize   Size of product in bytes
     * @return               Product-file corresponding to product-index
     */
    RcvProdFile getProdFile(
            const ProdId   prodId,
            const ProdSize prodSize) {
        Guard guard{mutex};
        auto  prodFile = getProdFile(prodId);

        if (!prodFile) {
            LOG_DEBUG("Creating product " + prodId.to_string());

            prodFile = RcvProdFile(rootFd, prodId, prodSize, segSize);
            addProdFile(prodId, prodFile);
        }

        return prodFile;
    }

    /**
     * Finishes processing a completely-received data-product by adding the product to the
     * completed-product queue
     *
     * @pre                 State is locked
     * @pre                 Product-file is complete
     * @param[in] prodFile  File containing the data-product
     */
    void finish(const RcvProdFile prodFile) {
        const auto prodInfo = prodFile.getProdInfo();
        Guard      guard(mutex);

        completeProds.push(prodInfo);
        cond.notify_all();
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize,
            const long         maxOpenFiles)
        : Repository::Impl{rootPathname, segSize, maxOpenFiles}
        , prodFiles() // TODO: Add existing files
        , openFiles(static_cast<size_t>(maxOpenFiles))
    {}

    /**
     * Saves product-information in the corresponding product-file. If the resulting product becomes
     * complete, then it will be eventually returned by `getNextProd()`.
     *
     * @param[in] rootFd       File descriptor open on repository's root directory
     * @param[in] prodInfo     Product information
     * @retval    `true`       This item was saved
     * @retval    `false`      This item was not saved because it already exists
     * @throws    SystemError  System failure
     * @see `getNextProd()`
     */
    bool save(const ProdInfo prodInfo)
    {
        auto       prodFile = getProdFile(prodInfo.getId(), prodInfo.getSize());
        const bool wasSaved = prodFile.save(rootFd, prodInfo);

        if (wasSaved && prodFile.isComplete())
            finish(prodFile);

        return wasSaved;
    }

    /**
     * Saves a data-segment in the corresponding product-file. If the resulting product becomes
     * complete, then it will be eventually returned by `getNextProd()`.
     *
     * @param[in] dataSeg   Data segment
     * @retval    `true`    This item was saved
     * @retval    `false`   This item was not saved because it already exists
     * @see `getNextProd()`
     */
    bool save(const DataSeg dataSeg)
    {
        auto       prodFile = getProdFile(dataSeg.getId().prodId, dataSeg.getProdSize());
        const auto wasSaved = prodFile.save(dataSeg);

        if (wasSaved && prodFile.isComplete())
            finish(prodFile);

        return wasSaved;
    }

    /**
     * Returns information on the next, completed data-product. Blocks until one is available.
     *
     * @return Next, completed data-product
     */
    ProdInfo getNextProd()
    {
        Lock lock(mutex);

        cond.wait(lock, [&]{return !completeProds.empty();});

        auto prodInfo = completeProds.front();
        completeProds.pop();

        return prodInfo;
    }

    /**
     * Returns information on a product, if it exists, given the product's ID.
     *
     * @param prodId     Product identifier
     * @return           Product information. Will test false if no such product
     *                   exists.
     */
    ProdInfo getProdInfo(const ProdId prodId)
    {
        Guard guard{mutex};
        auto  prodFile = getProdFile(prodId);

        if (!prodFile) {
            static const ProdInfo prodInfo{};
            return prodInfo;
        }

        return prodFile.getProdInfo();
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
        auto        offset = segId.offset;
        Guard       guard{mutex};
        RcvProdFile prodFile = getProdFile(segId.prodId);

        if (!prodFile) {
            static const DataSeg dataSeg{};
            return dataSeg;
        }

        return  DataSeg(segId, prodFile.getProdSize(),
                prodFile.getData(offset));
    }

    /**
     * Indicates if information on a data-product exists.
     *
     * @param[in] prodId     Product identifier
     * @retval    `true`     Product information does exist
     * @retval    `false`    Product information does not exist
     */
    bool exists(const ProdId prodId)
    {
        Guard       guard{mutex};
        RcvProdFile prodFile = getProdFile(prodId);

        return prodFile && prodFile.getProdInfo();
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
        Guard       guard{mutex};
        RcvProdFile prodFile = getProdFile(segId.prodId);

        return prodFile && prodFile.exists(segId.offset);
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
