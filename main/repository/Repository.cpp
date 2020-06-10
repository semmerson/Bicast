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
#include "LinkedMap.h"
#include "ProdFile.h"
#include "Thread.h"
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

typedef std::mutex              Mutex;
typedef std::lock_guard<Mutex>  Guard;
typedef std::unique_lock<Mutex> Lock;
typedef std::condition_variable Cond;

template<class PE> class ProdFiles; // Forward declaration

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

class RcvProdEntry final : public ProdEntry<RcvProdFile>
{
public:
    RcvProdEntry()
        : ProdEntry()
    {}

    RcvProdEntry(
            const ProdInfo&     prodInfo,
            struct RcvProdFile& prodFile)
        : ProdEntry(prodInfo, prodFile)
    {}

    RcvProdEntry(
            const ProdIndex     prodIndex,
            struct RcvProdFile& prodFile)
        : ProdEntry(ProdInfo(prodIndex, prodFile.getProdSize(), ""), prodFile)
    {}

    inline void setProdName(const std::string& name)
    {
        prodInfo = ProdInfo(prodInfo.getProdIndex(), prodInfo.getProdSize(),
                name);
    }

    bool isComplete() const
    {
        return prodInfo && !prodInfo.getProdName().empty() &&
                prodFile.isComplete();
    }
};

/******************************************************************************/

/**
 * Queue of open product-files.
 *
 * @tparam PF  Product-file type
 */
template <class PE>
class ProdFiles
{
    typedef std::unordered_map<ProdIndex,PE> Map;

    size_t    maxOpenFiles; ///< Maximum number of open product-files
    Map       prodFiles;    ///< Product files
    ProdIndex headIndex;    ///< Index of product at head of queue
    ProdIndex tailIndex;    ///< Index of product at tail of queue

    void removeFromQueue(PE& prodEntry)
    {
        if (prodEntry.prev)
            prodFiles[prodEntry.prev].next = prodEntry.next;
        if (prodEntry.next)
            prodFiles[prodEntry.next].prev = prodEntry.prev;
        if (prodEntry.prodInfo.getProdIndex() == headIndex)
            headIndex = prodEntry.next;
        if (prodEntry.prodInfo.getProdIndex() == tailIndex)
            tailIndex = prodEntry.prev;
    }

    inline void ensureRoom()
    {
        while (prodFiles.size() >= maxOpenFiles)
            removeFromQueue(prodFiles[headIndex]);
    }

    void addToTail(PE& prodEntry)
    {
        // Modify product-entry
        prodEntry.prev = tailIndex;
        prodEntry.next = ProdIndex();
        prodEntry.when = time(nullptr);

        // Modify tail of non-empty queue
        if (tailIndex)
            prodFiles[tailIndex].next = prodEntry.prodInfo.getProdIndex();
        tailIndex = prodEntry.prodInfo.getProdIndex();

        // Modify head of empty queue
        if (!headIndex)
            headIndex = tailIndex;
    }

    inline void moveToTail(PE& prodEntry)
    {
        removeFromQueue(prodEntry);
        addToTail(prodEntry);
    }

public:
    ProdFiles(const size_t maxSize = ::sysconf(_SC_OPEN_MAX)/2)
        : maxOpenFiles(maxSize)
        , prodFiles(maxSize)
        , headIndex{}
        , tailIndex{}
    {
        if (maxSize == 0)
            throw INVALID_ARGUMENT("Maximum size is 0");
    }

    /**
     * Adds a product-entry.
     *
     * @param[in] prodEntry  Product entry
     * @return               Pointer to added or previously-existing entry
     * @threadsafety         Compatible but unsafe
     */
    PE* add(PE& prodEntry)
    {
        auto  pair = prodFiles.emplace(std::piecewise_construct,
                std::forward_as_tuple(prodEntry.getProdInfo().getProdIndex()),
                std::forward_as_tuple(prodEntry));

        if (pair.second) {
            ensureRoom();
            addToTail(pair.first->second);
        }

        return &pair.first->second;
    }

    /**
     * Returns the product-entry corresponding to a product-index.
     *
     * @param[in] prodIndex  Product index
     * @retval    `nullptr`  No such entry
     * @return               Pointer to entry corresponding to index
     * @threadsafety         Compatible but unsafe
     */
    PE* find(const ProdIndex prodIndex)
    {
        auto  iter = prodFiles.find(prodIndex);

        if (iter == prodFiles.end())
            return nullptr;

        PE& prodEntry = iter->second;
        moveToTail(prodEntry);

        return &prodEntry;
    }

    /**
     * Deletes the entry corresponding to a product-index.
     *
     * @param[in] prodIndex  Product index
     * @threadsafety         Compatible but unsafe
     */
    void erase(const ProdIndex prodIndex)
    {
        auto  iter = prodFiles.find(prodIndex);

        if (iter != prodFiles.end()) {
            auto& prodEntry = iter->second;

            if (!prodEntry.isComplete())
                LOG_WARN("Closing incomplete product-file %s",
                        prodEntry.to_string().data());

            removeFromQueue(prodEntry);
            prodFiles.erase(iter); // Destructor closes file
        }
    }
};

/******************************************************************************/

/**
 * Abstract implementation of a repository of temporary data-products.
 */
class Repository::Impl
{
protected:
    mutable Mutex     mutex;        ///< For concurrency control
    const std::string rootPathname; ///< Pathname of root of repository

    /**
     * Pathname of root of directory hierarchy of product files based on
     * product-indexes
     */
    const std::string indexesDir;

    /**
     * Pathname of root of directory hierarchy of product files based on
     * product-names
     */
    const std::string namesDir;

    const SegSize     segSize;      ///< Size of canonical data-segment in bytes

    /**
     * Returns the pathname corresponding to a product-index relative to the
     * root directory for such pathnames. Beginning with the most significant
     * byte of the index, each byte is hex-encoded into a directory component --
     * except for the least significant byte which is encoded into the filename.
     * For example, for a four-byte product index, the pathname corresponding to
     * product-index 0xfedcba98 is "fe/dc/ba/98".
     *
     * @param prodIndex  Product index
     * @return           Corresponding relative pathname
     */
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

        return std::string(buf);
    }

    static void link(
            const std::string& extantPath,
            const std::string& newPath)
    {
        ensureDir(dirPath(newPath), 0700);

        if (::link(extantPath.data(), newPath.data()))
            throw SYSTEM_ERROR("Couldn't link \"" + newPath + "\" to \"" +
                    extantPath + "\"");
    }

    std::string getProdName(const std::string& namePath) const
    {
        return namePath.substr(namesDir.length()+1); // Remove `namesDir+"/"`
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize)
        : rootPathname{rootPathname}
        , indexesDir{rootPathname + "/indexes"}
        , namesDir{rootPathname + "/names"}
        , segSize{segSize}
        , mutex{}
    {
        int status = ::mkdir(rootPathname.data(), 0777);
        if (status && errno != EEXIST)
            throw SYSTEM_ERROR("Couldn't create repository \"" + rootPathname +
                    "\"");

        status = ::mkdir(namesDir.data(), 0777);
        if (status && errno != EEXIST)
            throw SYSTEM_ERROR("Couldn't create name-directory \"" + namesDir +
                    "\"");

        status = ::mkdir(indexesDir.data(), 0777);
        if (status && errno != EEXIST)
            throw SYSTEM_ERROR("Couldn't create indexes-directory \"" +
                    indexesDir + "\"");
    }

    virtual ~Impl() noexcept
    {}

    SegSize getSegSize() const noexcept
    {
        return segSize;
    }

    const std::string& getRootDir() const noexcept
    {
        return rootPathname;
    }

    std::string getPathname(const ProdIndex prodIndex) const
    {
        return indexesDir + "/" + getIndexPath(prodIndex);
    }

    std::string getPathname(const std::string& name) const
    {
        return namesDir + "/" + name;
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

Repository::Repository(Impl* impl)
    : pImpl{impl} {
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
    ProdFiles<SndProdEntry> prodFiles;   ///< Active Product files
    Mutex                   mutex;       ///< For concurrent access
    Cond                    cond;        ///< For product-index queue
    Watcher                 watcher;     ///< Watches filename hierarchy
    std::queue<ProdIndex>   prodIndexes; ///< Queue of products to be sent
    ProdIndex               prodIndex;   ///< Next product-index

    ProdIndex getNextIndex()
    {
        // TODO: Make persistent between sessions
        return ++prodIndex;
    }

    /**
     * Creates a product-entry and adds it to `prodFiles`.
     *
     * @param[in] prodName   Product name
     * @param[in] prodIndex  Product index
     * @return               Pointer to added or previously-existing
     *                       product-entry
     */
    SndProdEntry* addProdEntry(
            const std::string& prodName,
            const ProdIndex    prodIndex)
    {
        const auto   indexPath = getPathname(prodIndex);
        SndProdFile  prodFile(indexPath, segSize);
        SndProdEntry prodEntry(prodName, prodIndex, prodFile);

        return prodFiles.add(prodEntry);
    }

    /**
     * Returns an active product-entry. If the product-entry isn't in the set of
     * active product-entries, then one is created and added.
     *
     * @param[in] prodIndex  Product-index of product-entry
     * @retval    `nullptr`  No such entry exists
     * @return               Pointer to the product-entry
     */
    SndProdEntry* getProdEntry(const ProdIndex prodIndex)
    {
        SndProdEntry* prodEntry = prodFiles.find(prodIndex);

        if (prodEntry == nullptr) {
            char       namePath[PATH_MAX];
            const auto indexPath = getPathname(prodIndex);
            ssize_t    nbytes = ::readlink(indexPath.data(), namePath,
                    sizeof(namePath));

            if (nbytes != -1) {
                namePath[nbytes] = 0;
                prodEntry = addProdEntry(
                        std::string(namePath+namesDir.size()+1), prodIndex);
            }
        }

        return prodEntry;
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize,
            const size_t       maxOpenFiles = sysconf(_SC_OPEN_MAX)/2)
        : Repository::Impl{rootPathname, segSize}
        , prodFiles(maxOpenFiles)
        , mutex()
        , watcher(namesDir)
        , prodIndex()
    {}

    /**
     * Links to a file (which could be a directory) that's outside the
     * repository. The watcher will notice and process the link.
     *
     * @param[in] filePath  Pathname of outside file
     * @param[in] prodName  Name of product
     */
    void link(
            const std::string& filePath,
            const std::string& prodName)
    {
        Guard guard(mutex);

        const std::string linkPath = getPathname(prodName);
        int               status = ::link(filePath.data(), linkPath.data());

        if (status == -1 && symlink(filePath.data(), linkPath.data()))
            throw SYSTEM_ERROR("Couldn't link to file \"" + filePath +
                    "\" from \"" + linkPath + "\"");
    }

    /**
     * Returns the index of the next product to publish. Blocks until one is
     * ready. Watches the product-name directory hierarchy for new files. For
     * each new non-directory file in the product-name hierarchy:
     *   - A symbolic-link is created in the product-index hierarchy that
     *     references the named file; and
     *   - A product-entry is created and added to the set of active
     *     product-entries.
     *
     * @return              Index of next product to publish
     * @throws SystemError  System failure
     * @threadsafety        Compatible but unsafe
     */
    ProdIndex getNextProd()
    {
        ProdIndex prodIndex = getNextIndex();

        try {
            Watcher::WatchEvent event;
            watcher.getEvent(event);

            // Event pathname has `namesDir+"/"` prefix
            const auto  indexPath = getPathname(prodIndex);
            const auto& namePath = event.pathname;
            const auto  prodName = namePath.substr(namesDir.size()+1);

            {
                Canceler canceler(false);
                if (::symlink(namePath.data(), indexPath.data()))
                    throw SYSTEM_ERROR("Couldn't link \"" + namePath +
                            " from \"" + indexPath + "\"");
            }

            // Create an active product-entry
            Lock lock(mutex);
            addProdEntry(prodName, prodIndex);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't get next "
                    "product-file"));
        }

        return prodIndex;
    }

    /**
     * Returns the product-information that corresponds to a product-index.
     *
     * @param[in] prodIndex  Product index
     * @return               Corresponding product-information. Will test false
     *                       if no such information exists.
     */
    ProdInfo getProdInfo(const ProdIndex prodIndex)
    {
        Guard guard{mutex};

        const auto prodEntry = getProdEntry(prodIndex);

        return (prodEntry == nullptr)
                ? ProdInfo{}
                : prodEntry->getProdInfo();
    }

    /**
     * Returns the in-memory data-segment that corresponds to a
     * segment-identifier.
     *
     * @param[in] segId  Data-segment identifier
     * @return           Corresponding in-memory data-segment Will test false if
     *                   no such segment exists.
     */
    MemSeg getMemSeg(const SegId& segId)
    {
        Guard      guard{mutex};
        const auto prodEntry = getProdEntry(segId.getProdIndex());

        if (prodEntry == nullptr)
            return MemSeg{};

        auto prodFile = prodEntry->getProdFile();
        auto offset = segId.getOffset();

        return MemSeg(SegInfo(segId, prodFile.getProdSize(),
                        prodFile.getSegSize(offset)),
                    prodFile.getData(offset));
    }
};

/******************************************************************************/

PubRepo::PubRepo(
        const std::string& root,
        const SegSize      segSize)
    : Repository{new Impl(root, segSize)} {
}

void PubRepo::link(
        const std::string& filePath,
        const std::string& prodName) {
    return static_cast<Impl*>(pImpl.get())->link(filePath, prodName);
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
    typedef LinkedMap<ProdIndex, RcvProdFile> LinkedProdMap;

    LinkedProdMap        prodFiles;     ///< All existing product-files
    LinkedProdMap        openFiles;     ///< All open product-files
    size_t               maxOpenFiles;  ///< Max number open files
    Cond                 cond;          ///< Concurrency condition-variable
    std::queue<ProdInfo> completeProds; ///< Queue of completed products

    void link(const ProdInfo& prodInfo)
    {
        const auto indexPath = getPathname(prodInfo.getProdIndex());
        const auto namePath = getPathname(prodInfo.getProdName());

        Repository::Impl::link(indexPath, namePath);
    }

    void makeRoom()
    {
        while (openFiles.size() >= maxOpenFiles)
            openFiles.remove(openFiles.getHead()).close();
    }

    /**
     * Returns the product-file that corresponds to a product-index. The
     * product-file is open. The product-file is added or moved to the tail-end
     * of the open-files list.
     *
     * @param[in] prodIndex  Product-index
     * @retval    `nullptr`  No such product-file
     * @return               Open product-file corresponding to product-index
     */
    RcvProdFile* getProdFile(const ProdIndex prodIndex)
    {
        RcvProdFile* prodFile = openFiles.find(prodIndex);

        if (prodFile == nullptr) {
            prodFile = prodFiles.find(prodIndex);

            if (prodFile) {
                makeRoom();
                prodFile->open();
                openFiles.add(prodIndex, *prodFile);
            }
        }

        return prodFile;
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize)
        : Repository::Impl{rootPathname, segSize}
        , prodFiles() // TODO: Add existing files
        , openFiles(maxOpenFiles)
        , maxOpenFiles(maxOpenFiles)
    {
        if (maxOpenFiles == 0)
            throw INVALID_ARGUMENT("Maximum number of open files is zero");
    }

    bool save(const ProdInfo& prodInfo)
    {
        bool       wasSaved = false;
        const auto prodIndex = prodInfo.getProdIndex();
        Guard      guard{mutex};
        auto       prodFile = getProdFile(prodIndex);

        if (prodFile) {
            // Existing product-file
            auto& prodName = prodInfo.getProdName();
            LOG_DEBUG("Saving product-information %s",
                    prodInfo.to_string().data());
            if (!prodFile->getProdInfo()) {
                prodFile->setProdInfo(prodInfo);
                wasSaved = true;
            }
        }
        else {
            // New product-file
            LOG_DEBUG("Creating product %s", prodInfo.to_string().data());

            RcvProdFile file{getPathname(prodIndex), prodInfo.getProdSize(),
                segSize};

            prodFiles.add(prodIndex, file);
            prodFile = &openFiles.add(prodIndex, file).first;
            wasSaved = true;
        }

        if (wasSaved && prodFile->isComplete()) {
            Repository::Impl::link(prodFile->getPathname(),
                    getPathname(prodInfo.getProdName()));
            completeProds.push(prodInfo);
            cond.notify_one();
        }

        return true;
    }

    bool save(DataSeg& dataSeg)
    {
        bool            wasSaved = false;
        const auto      segInfo = dataSeg.getSegInfo();
        const ProdIndex prodIndex = segInfo.getProdIndex();
        Guard           guard{mutex};
        auto            prodFile = getProdFile(prodIndex);

        if (prodFile) {
            // Existing product-file
            if (prodFile->exists(segInfo.getSegOffset())) {
                // Product-file already has the segment
                LOG_DEBUG("Ignoring data-segment %s",
                        dataSeg.to_string().data());
            }
            else {
                // Product-file doesn't have the segment
                LOG_DEBUG("Saving data-segment %s", dataSeg.to_string().data());
                prodFile->save(dataSeg);
                wasSaved = true;
            }
        }
        else {
            // New product-file
            LOG_DEBUG("Saving data-segment %s", dataSeg.to_string().data());
            RcvProdFile file{getPathname(prodIndex), segInfo.getProdSize(),
                segSize};

            file.save(dataSeg);
            prodFiles.add(prodIndex, file);
            prodFile = &openFiles.add(prodIndex, file).first;
            wasSaved = true;
        }

        if (wasSaved && prodFile->isComplete()) {
            const ProdInfo& prodInfo = prodFile->getProdInfo();

            Repository::Impl::link(prodFile->getPathname(),
                    getPathname(prodInfo.getProdName()));
            completeProds.push(prodInfo);
            cond.notify_one();
        }

        return wasSaved;
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

        ProdInfo prodInfo = completeProds.front();
        completeProds.pop();

        return prodInfo;
    }

    /**
     * Returns information on a product given the product's index.
     *
     * @param prodIndex  Product's index
     * @return           Product information. Will test false if no such product
     *                   exists.
     */
    ProdInfo getProdInfo(const ProdIndex prodIndex)
    {
        Guard        guard{mutex};
        RcvProdFile* prodFile = getProdFile(prodIndex);

        return prodFile
                ? prodFile->getProdInfo()
                : ProdInfo();
    }

    /**
     * Returns the memory data-segment corresponding to a segment identifier.
     *
     * @param[in] segId  Segment identifier
     * @return           Corresponding memory data-segment. Will test false if
     *                   no such segment exists.
     */
    MemSeg getMemSeg(const SegId& segId)
    {
        auto         offset = segId.getOffset();
        Guard        guard{mutex};
        RcvProdFile* prodFile = getProdFile(segId.getProdIndex());

        return prodFile
                ? MemSeg(SegInfo(segId, prodFile->getProdSize(),
                        prodFile->getSegSize(offset)),
                        prodFile->getData(offset))
                : MemSeg();
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
        Guard        guard{mutex};
        RcvProdFile* prodFile = getProdFile(prodIndex);

        return prodFile && prodFile->getProdInfo();
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
        Guard        guard{mutex};
        RcvProdFile* prodFile = getProdFile(segId.getProdIndex());

        return prodFile && prodFile->exists(segId.getOffset());
    }
};

/******************************************************************************/

SubRepo::SubRepo(
        const std::string& rootPathname,
        const SegSize      segSize)
    : Repository{new Impl(rootPathname, segSize)} {
}

bool SubRepo::save(const ProdInfo& prodInfo) const {
    return static_cast<Impl*>(pImpl.get())->save(prodInfo);
}

bool SubRepo::save(DataSeg& dataSeg) const {
    return static_cast<Impl*>(pImpl.get())->save(dataSeg);
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
