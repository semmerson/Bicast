/**
 * Repository of data-products.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Repository.cpp
 *  Created on: Dec 23, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "Repository.h"

#include "error.h"
#include "FileUtil.h"
#include "hycast.h"
#include "ProdFile.h"
#include "Thread.h"
#include "Watcher.h"

#include "LinkedMap.cpp"

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

typedef std::mutex              Mutex;
typedef std::lock_guard<Mutex>  Guard;
typedef std::unique_lock<Mutex> Lock;
typedef std::condition_variable Cond;

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
    ProdIndex prev;      ///< Previous entry in queue
    ProdIndex next;      ///< Next entry in queue

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
        : prodInfo{prodInfo}
        , prodFile{prodFile}
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
        return prodInfo.getProdName();
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
            const ProdIndex    prodIndex,
            SndProdFile&       prodFile)
        : ProdEntry(ProdInfo(prodIndex, prodFile.getProdSize(), prodName),
                prodFile)
    {}

    bool isComplete() const
    {
        return true;
    }
};

/******************************************************************************/

/**
 * Abstract, base implementation of a repository of temporary data-products.
 */
class Repository::Impl
{
protected:
    /**
     * A product-index to product-file hash table that also identifies the
     * least-recently-used product-file.
     *
     * @tparam PF  Type of product-file
     */
    template<class PF>
    class LinkedProdMap final
    {
        /**
         * A hash table entry for a product-file that also forms a linked-list.
         */
        struct Entry final
        {
            PF          prodFile; ///< Receiving product-file
            ProdIndex   prev;     ///< Previous entry (towards the head)
            ProdIndex   next;     ///< Subsequent entry (towards the tail)

            Entry()
                : prodFile()
                , prev()
                , next()
            {}

            Entry(PF& prodFile, const ProdIndex prev)
                : prodFile(prodFile)
                , prev(prev)
                , next()
            {}
        };

        std::unordered_map<ProdIndex, Entry> map;
        ProdIndex                            head; ///< Head of list
        ProdIndex                            tail; ///< Tail of list

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
        size_t size()
        {
            return map.size();
        }

        /**
         * Adds an entry. The entry will be at the tail-end of the list.
         *
         * @param[in] prodIndex   Product index
         * @param[in] prodFile    Associated product-file
         * @throws    LogicError  An entry already exists for the product-index
         */
        void add(
                const ProdIndex prodIndex,
                PF              prodFile)
        {
            if (!prodIndex)
                throw INVALID_ARGUMENT("Product-index is invalid");

            Entry                      entry(prodFile, tail);
            std::pair<ProdIndex,Entry> elt(prodIndex, entry);
            auto                       pair = map.insert(elt);

            if (!pair.second)
                throw LOGIC_ERROR("Entry already exists");

            if (tail) {
                map[tail].next = prodIndex;
            }
            else {
                head = prodIndex;
            }
            tail = prodIndex;
        }

        /**
         * Removes an entry.
         *
         * @param[in] prodIndex     Product index
         * @return                  Receiving product-file associated with
         *                          product-index
         * @throws InvalidArgument  No such entry
         */
        PF remove(const ProdIndex prodIndex)
        {
            auto iter = map.find(prodIndex);

            if (iter == map.end())
                throw INVALID_ARGUMENT("No such entry");

            Entry& entry = iter->second;

            if (entry.prev) {
                map[entry.prev].next = entry.next;
            }
            else {
                head = entry.next;
            }

            if (entry.next) {
                map[entry.next].prev = entry.prev;
            }
            else {
                tail = entry.prev;
            }

            auto prodFile = entry.prodFile;;

            map.erase(iter);

            return prodFile;
        }

        /**
         * Returns the product-file associated with a product-index if it
         * exists. The file will be at the end of the list.
         *
         * @param[in] prodIndex  Product index
         * @return               Associated product-file. Will test false if it
         *                       doesn't exist.
         */
        PF find(const ProdIndex prodIndex)
        {
            auto      iter = map.find(prodIndex);

            if (iter == map.end()) {
                static const PF prodFile{};
                return prodFile;
            }

            auto prodFile = remove(prodIndex);
            add(prodIndex, prodFile);
            return prodFile;
        }

        /**
         * Returns the product-index of the head of the list.
         *
         * @return Product-index of head of list. Will test false if the list is
         * empty.
         */
        ProdIndex getHead()
        {
            return head;
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
            const size_t       maxOpenFiles)
        : mutex{}
        , cond()
        , rootPathname{makeAbsolute(rootPathname)}
        , rootPrefixLen{rootPathname.size() + 1}
        , rootFd(-1)
        , segSize{segSize}
        , maxOpenFiles{maxOpenFiles}
    {
        ensureDir(rootPathname, 0755); // Only owner can write

        rootFd = ::open(rootPathname.data(), O_RDONLY);
        if (rootFd == -1)
            throw SYSTEM_ERROR("Couldn't open root-directory of repository, \""
                    + rootPathname + "\"");

        try {
            if (maxOpenFiles == 0)
                throw INVALID_ARGUMENT("Maximum number of open files is zero");
        } // `rootFd` is open
        catch (const std::exception& ex) {
            ::close(rootFd);
        }
    }

    /**
     * Ensures that there's one less than the maximum number of open product-
     * files.
     *
     * @pre State is locked
     */
    template<class PF>
    static void ensureRoom(
            LinkedProdMap<PF>& map,
            const size_t       maxOpenFiles) {
        while (map.size() >= maxOpenFiles) {
            const auto prodIndex = map.getHead();
            map.find(prodIndex).close();
            map.remove(prodIndex);
        }
    }

    static std::string getIndexPath(const ProdIndex prodIndex)
    {
        auto  index = prodIndex.getValue();
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
     * Returns the product-name corresponding to the absolute pathname of a
     * product-file.
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

    /**
     * Returns the absolute pathname of the file corresponding to a product
     * name.
     *
     * @pre                 `name.size() > 0`
     * @param[in] prodName  Product name
     * @return              Absolute pathname of corresponding file
     */
    std::string getPathname(const std::string& name) const
    {
        assert(name.size());
        return rootPathname + "/" + name;
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

        ProdIndex next;
        for (auto prodIndex = headIndex; prodIndex; prodIndex = next) {
            auto iter = prodFiles.find(prodIndex);
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

Repository::Repository(Impl* impl) noexcept
    : pImpl(impl) {
}

SegSize Repository::getSegSize() const noexcept {
    return pImpl->getSegSize();
}

const std::string& Repository::getRootDir() const noexcept {
    return pImpl->getRootDir();
}

/******************************************************************************/
/******************************************************************************/

/**
 * Implementation of a publisher's repository.
 */
class PubRepo::Impl final : public Repository::Impl
{
    LinkedProdMap<SndProdFile> prodFiles; ///< All existing product-files
    LinkedProdMap<SndProdFile> openFiles; ///< All open product-files
    Watcher                    watcher;   ///< Watches filename hierarchy
    std::queue<ProdIndex>      prodQueue; ///< Queue of products to be sent
    ProdIndex                  prodIndex; ///< Next product-index

    ProdIndex getNextIndex()
    {
        // TODO: Make persistent between sessions
        return ++prodIndex;
    }

    /**
     * Ensures that there's one less than the maximum number of open product-
     * files.
     *
     * @pre State is locked
    void ensureRoom() {
        while (openFiles.size() >= maxOpenFiles) {
            const auto prodIndex = openFiles.getHead();
            openFiles.find(prodIndex).close();
            openFiles.remove(prodIndex);
        }
    }
     */

    /**
     * Adds a new product-file.
     *
     * @pre                  State is unlocked
     * @param[in] prodIndex  Product-index
     * @param[in] prodFile   Product-file
     */
    void addProdFile(
            const ProdIndex prodIndex,
            SndProdFile&    prodFile) {
        Guard guard(mutex);

        prodFiles.add(prodIndex, prodFile);
        ensureRoom<SndProdFile>(openFiles, maxOpenFiles);
        openFiles.add(prodIndex, prodFile);
        prodQueue.push(prodIndex);
        cond.notify_all();
    }

    /**
     * Returns the product-file corresponding to a product-index. The product-
     * file is open and at the tail-end of the open-files list.
     *
     * @pre                  State is locked
     * @param[in] prodIndex  Product-index of product-entry
     * @return               Corresponding product-file. Will test false if it
     *                       doesn't exist.
     */
    SndProdFile getProdFile(const ProdIndex prodIndex)
    {
        assert(!mutex.try_lock());

        auto prodFile = openFiles.find(prodIndex);

        if (!prodFile) {
            prodFile = prodFiles.find(prodIndex);

            if (prodFile) {
                prodFile.open(rootFd);
                ensureRoom<SndProdFile>(openFiles, maxOpenFiles);
                openFiles.add(prodIndex, prodFile);
            }
        }

        return prodFile;
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize,
            const size_t       maxOpenFiles = sysconf(_SC_OPEN_MAX)/2)
        : Repository::Impl{rootPathname, segSize, maxOpenFiles}
        , prodFiles(maxOpenFiles)
        , watcher(this->rootPathname) // Absolute pathname
        , prodQueue()
        , prodIndex()
    {}

    /**
     * Links to a file (which could be a directory) that's outside the
     * repository. The watcher will notice and process the link.
     *
     * @param[in] pathname       Absolute pathname (with no trailing '/') of the
     *                           file or directory to be linked to
     * @param[in] prodName       Product name if the pathname references a file
     *                           and Product name prefix if the pathname
     *                           references a directory
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
            throw INVALID_ARGUMENT("Invalid product name: \"" + prodName +
                    "\"");

        const std::string extantPath = makeAbsolute(pathname);
        const std::string linkPath = getPathname(prodName);
        Guard             guard(mutex);

        ensureDir(dirPath(linkPath), 0700);

        if (!tryHardLink(extantPath, linkPath) &&
                !trySoftLink(extantPath, linkPath))
            throw SYSTEM_ERROR("Couldn't link file \"" + linkPath +
                    "\" to \"" + extantPath + "\"");
    }

    /**
     * Returns the index of the next product to publish. Blocks until one is
     * ready. Watches the repository's directory hierarchy for new files. A
     * product-entry is created and added to the set of active product-entries.
     * for each new non-directory file in the repository's hierarchy:
     *
     * @return              Index of next product to publish
     * @throws SystemError  System failure
     * @threadsafety        Compatible but unsafe
     */
    ProdIndex getNextProd()
    {
        auto prodIndex = getNextIndex();

        try {
            Watcher::WatchEvent event;
            watcher.getEvent(event);

            const auto prodName = event.pathname.substr(rootPrefixLen);

            SndProdFile prodFile(rootFd, prodName, segSize);
            addProdFile(prodIndex, prodFile);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't get product-file "
                    "corresponding to product-index " + prodIndex.to_string()));
        }

        return prodIndex;
    }

    /**
     * Returns the product-information corresponding to a product-index.
     *
     * @param[in] prodIndex  Product index
     * @return               Corresponding product-information. Will test false
     *                       if no such information exists.
     */
    const ProdInfo getProdInfo(const ProdIndex prodIndex)
    {
        Guard      guard{mutex};
        const auto prodFile = getProdFile(prodIndex);

        if (!prodFile) {
            static const ProdInfo prodInfo{};
            return prodInfo;
        }

        return ProdInfo(prodIndex, prodFile.getProdSize(),
                prodFile.getPathname());
    }

    /**
     * Returns the in-memory data-segment that corresponds to a segment
     * identifier.
     *
     * @param[in] segId  Data-segment identifier
     * @return           Corresponding in-memory data-segment Will test false if
     *                   no such segment exists.
     */
    MemSeg getMemSeg(const SegId& segId)
    {
        Guard      guard{mutex};
        const auto prodFile = getProdFile(segId.getProdIndex());

        if (!prodFile) {
            static const MemSeg memSeg{};
            return memSeg;
        }

        auto offset = segId.getOffset();

        return MemSeg(SegInfo(segId, prodFile.getProdSize(),
                prodFile.getSegSize(offset)), prodFile.getData(offset));
    }
};

/******************************************************************************/

PubRepo::PubRepo(
        const std::string& rootPathname,
        const SegSize      segSize,
        const size_t       maxOpenFiles)
    : Repository{new Impl(rootPathname, segSize, maxOpenFiles)} {
}

void PubRepo::link(
        const std::string& pathname,
        const std::string& prodName) {
    return static_cast<Impl*>(pImpl.get())->link(pathname, prodName);
}

ProdIndex PubRepo::getNextProd() const {
    return static_cast<Impl*>(pImpl.get())->getNextProd();
}

ProdInfo PubRepo::getProdInfo(const ProdIndex prodIndex) const {
    return static_cast<Impl*>(pImpl.get())->getProdInfo(prodIndex);
}

MemSeg PubRepo::getMemSeg(const SegId& segId) const {
    return static_cast<Impl*>(pImpl.get())->getMemSeg(segId);
}

/******************************************************************************/
/******************************************************************************/

/**
 * Implementation of a subscriber's repository.
 */
class SubRepo::Impl final : public Repository::Impl
{
    Cond                       cond;          ///< Concurrency condition-variable
    std::queue<ProdInfo>       completeProds; ///< Queue of completed products
    LinkedProdMap<RcvProdFile> prodFiles;
    LinkedProdMap<RcvProdFile> openFiles;

    void makeRoom()
    {
        while (openFiles.size() >= maxOpenFiles)
            openFiles.remove(openFiles.getHead()).close();
    }

    /**
     * Returns the product-file corresponding to a product-index if it exists.
     * The file is
     *   - Open
     *   - At the tail-end of the open-files list
     *
     * @pre                  State is locked
     * @param[in] prodIndex  Product-index
     * @return               Product-file corresponding to product-index.
     *                       Will test false if it doesn't exist.
     */
    RcvProdFile getProdFile(const ProdIndex prodIndex)
    {
        assert(!mutex.try_lock());

        RcvProdFile prodFile = openFiles.find(prodIndex);

        if (!prodFile) {
            prodFile = prodFiles.find(prodIndex);

            if (prodFile) {
                ensureRoom<RcvProdFile>(openFiles, maxOpenFiles);
                prodFile.open(rootFd);
                openFiles.add(prodIndex, prodFile);
            }
        }

        return prodFile;
    }

    /**
     * Adds a new product-file.
     *
     * @pre                  State is locked
     * @param[in] prodIndex  Product-index
     * @param[in] prodFile   Product-file
     */
    void addProdFile(
            const ProdIndex prodIndex,
            RcvProdFile&    prodFile) {
        assert(!mutex.try_lock());

        prodFiles.add(prodIndex, prodFile);
        openFiles.add(prodIndex, prodFile);
    }

    /**
     * Returns the product-file that corresponds to a product-index. The file is
     *   - Created if necessary
     *   - Open
     *   - At the tail-end of the open-files list
     *
     * @pre                  State is unlocked
     * @param[in] prodIndex  Product-index
     * @param[in] prodSize   Size of product in bytes
     * @return               Product-file corresponding to product-index
     */
    RcvProdFile getProdFile(
            const ProdIndex prodIndex,
            const ProdSize  prodSize) {
        Guard guard{mutex};
        auto  prodFile = getProdFile(prodIndex);

        if (!prodFile) {
            LOG_DEBUG("Creating product " + prodIndex.to_string());

            prodFile = RcvProdFile(rootFd, prodIndex, prodSize, segSize);
            addProdFile(prodIndex, prodFile);
        }

        return prodFile;
    }

    /**
     * Finishes processing a completely-received data-product by adding the
     * product to the completed-product queue
     *
     * @pre                 State is locked
     * @pre                 Product-file is complete
     * @param[in] prodFile  File containing the data-product
     */
    void finish(const RcvProdFile& prodFile) {
        const auto prodInfo = prodFile.getProdInfo();
        Guard      guard(mutex);

        completeProds.push(prodInfo);
        cond.notify_all();
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize,
            const size_t       maxOpenFiles)
        : Repository::Impl{rootPathname, segSize, maxOpenFiles}
        , prodFiles() // TODO: Add existing files
        , openFiles(maxOpenFiles)
    {}

    /**
     * Saves product-information in the corresponding product-file. If the
     * resulting product becomes complete, then it will be eventually returned
     * by `getCompleted()`.
     *
     * @param[in] rootFd       File descriptor open on repository's root
     *                         directory
     * @param[in] prodInfo     Product information
     * @retval    `true`       This item completed the product
     * @retval    `false`      This item did not complete the product
     * @throws    SystemError  System failure
     * @see `getCompleted()`
     */
    bool save(const ProdInfo& prodInfo)
    {
        auto       prodFile = getProdFile(prodInfo.getProdIndex(),
                prodInfo.getProdSize());
        const bool bingo = prodFile.save(rootFd, prodInfo);

        if (bingo)
            finish(prodFile);

        return bingo;
    }

    /**
     * Saves a data-segment in the corresponding product-file. If the resulting
     * product becomes complete, then it will be eventually returned by
     * `getCompleted()`.
     *
     * @param[in] dataSeg   Data segment
     * @retval    `true`    This item completed the product
     * @retval    `false`   This item did not complete the product
     * @see `getCompleted()`
     */
    bool save(DataSeg& dataSeg)
    {
        auto       prodFile = getProdFile(dataSeg.getProdIndex(),
                dataSeg.getProdSize());
        const auto bingo = prodFile.save(dataSeg);

        if (bingo)
            finish(prodFile);

        return bingo;
    }

    /**
     * Returns the next, completed data-product. Blocks until one is available.
     *
     * @return Next, completed data-product
     */
    ProdInfo getCompleted()
    {
        Lock lock(mutex);

        while(completeProds.empty())
            cond.wait(lock);

        auto prodInfo = completeProds.front();
        completeProds.pop();

        return prodInfo;
    }

    /**
     * Returns information on a product, if it exists, given the product's
     * index.
     *
     * @param prodIndex  Product's index
     * @return           Product information. Will test false if no such product
     *                   exists.
     */
    ProdInfo getProdInfo(const ProdIndex prodIndex)
    {
        Guard                 guard{mutex};
        auto                  prodFile = getProdFile(prodIndex);

        if (!prodFile) {
            static const ProdInfo prodInfo{};
            return prodInfo;
        }

        return prodFile.getProdInfo();
    }

    /**
     * Returns the memory data-segment corresponding to a segment identifier if
     * it exists.
     *
     * @param[in] segId  Segment identifier
     * @return           Corresponding memory data-segment. Will test false if
     *                   no such segment exists.
     */
    MemSeg getMemSeg(const SegId& segId)
    {
        auto                offset = segId.getOffset();
        Guard               guard{mutex};
        RcvProdFile         prodFile = getProdFile(segId.getProdIndex());

        if (!prodFile) {
            static const MemSeg memSeg{};
            return memSeg;
        }

        return  MemSeg(SegInfo(segId, prodFile.getProdSize(),
                        prodFile.getSegSize(offset)),
                        prodFile.getData(offset));
    }

    /**
     * Indicates if information on a data-product exists.
     *
     * @param[in] prodIndex  Product index
     * @retval    `true`     Product information does exist
     * @retval    `false`    Product information does not exist
     */
    bool exists(const ProdIndex prodIndex)
    {
        Guard       guard{mutex};
        RcvProdFile prodFile = getProdFile(prodIndex);

        return prodFile && prodFile.getProdInfo();
    }

    /**
     * Indicates if a data-segment exists.
     *
     * @param[in] prodIndex  Product index
     * @retval    `true`     Data-segment does exist
     * @retval    `false`    Data-segment does not exist
     */
    bool exists(const SegId& segId)
    {
        Guard       guard{mutex};
        RcvProdFile prodFile = getProdFile(segId.getProdIndex());

        return prodFile && prodFile.exists(segId.getOffset());
    }
};

/******************************************************************************/

SubRepo::SubRepo(
        const std::string& rootPathname,
        const SegSize      segSize,
        const size_t       maxOpenFiles)
    : Repository{new Impl(rootPathname, segSize, maxOpenFiles)} {
}

bool SubRepo::save(const ProdInfo& prodInfo) const {
    return static_cast<Impl*>(pImpl.get())->save(prodInfo);
}

bool SubRepo::save(DataSeg& dataSeg) const {
    return static_cast<Impl*>(pImpl.get())->save(dataSeg);
}

ProdInfo SubRepo::getCompleted() const {
    return static_cast<Impl*>(pImpl.get())->getCompleted();
}

ProdInfo SubRepo::getProdInfo(const ProdIndex prodIndex) const {
    return static_cast<Impl*>(pImpl.get())->getProdInfo(prodIndex);
}

MemSeg SubRepo::getMemSeg(const SegId& segId) const {
    return static_cast<Impl*>(pImpl.get())->getMemSeg(segId);
}

bool SubRepo::exists(const ProdIndex prodIndex) const {
    return static_cast<Impl*>(pImpl.get())->exists(prodIndex);
}

bool SubRepo::exists(const SegId& segId) const {
    return static_cast<Impl*>(pImpl.get())->exists(segId);
}

} // namespace
