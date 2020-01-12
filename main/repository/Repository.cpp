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

#include <cstring>
#include <errno.h>
#include <functional>
#include <libgen.h>
#include <limits.h>
#include <mutex>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <unordered_map>

namespace hycast {

typedef struct ProdEntry
{
    std::string namePath;  ///< Pathname of product-file based on product-name
    time_t      when;      ///< Creation-time of entry
    ProdIndex   prodIndex; ///< Product index
    ProdIndex   prev;      ///< Previous entry in queue
    ProdIndex   next;      ///< Next entry in queue

    ProdEntry()
        : namePath{}
        , when{time(nullptr)}
        , prodIndex{}
        , prev{}
        , next{}
    {}

    ProdEntry(const ProdIndex prodIndex)
        : namePath{}
        , when{time(nullptr)}
        , prodIndex{prodIndex}
        , prev{}
        , next{}
    {}

    virtual ~ProdEntry() noexcept
    {}

    ProdEntry& operator =(const ProdEntry& rhs)
    {
        namePath = rhs.namePath;
        when = rhs.when;
        prodIndex = rhs.prodIndex;
        prev = rhs.prev;
        next = rhs.next;
        return *this;
    }

    virtual bool isComplete() const =0;
} ProdEntry;

class Repository::Impl
{
protected:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;

    const std::string rootPathname; ///< Pathname of root of repository
    /// Pathname of root of directory hierarchy of product files based on
    /// product-indexes
    const std::string indexesDir;
    /// Pathname of root of directory hierarchy of product files based on
    /// product-names
    const std::string namesDir;
    const SegSize     segSize;      ///< Size of canonical data-segment in bytes
    mutable Mutex     mutex;        ///< For concurrency control
    ProdIndex         headIndex;    ///< Index of product at head of queue
    ProdIndex         tailIndex;    ///< Index of product at tail of queue

    static std::string getIndexPath(const ProdIndex prodId)
    {
        auto  id = prodId.getValue();
        char  buf[sizeof(id)*3 + 1];

        for (int i = 0; i < sizeof(id); ++i) {
            (void)sprintf(buf+3*i, "/%.2x", id & 0xff);
            id >>= 8;
        }

        return std::string(buf);
    }

    template <class MAP, class ENTRY>
    void removeFromList(
            MAP&   prodFiles,
            ENTRY& prodEntry)
    {
        if (prodEntry.prev)
            prodFiles[prodEntry.prev].next = prodEntry.next;
        if (prodEntry.next)
            prodFiles[prodEntry.next].prev = prodEntry.prev;
        if (prodEntry.prodIndex == headIndex)
            headIndex = prodEntry.next;
        if (prodEntry.prodIndex == tailIndex)
            tailIndex = prodEntry.prev;
    }

    template <class MAP, class ENTRY>
    void addToTail(
            MAP&   prodFiles,
            ENTRY& prodEntry)
    {
        // Modify product-entry
        prodEntry.prev = tailIndex;
        prodEntry.next = ProdIndex();
        prodEntry.when = time(nullptr);

        // Modify tail of non-empty queue
        if (tailIndex)
            prodFiles[tailIndex].next = prodEntry.prodIndex;
        tailIndex = prodEntry.prodIndex;

        // Modify head of empty queue
        if (!headIndex)
            headIndex = tailIndex;
    }

    template <class MAP, class ENTRY>
    inline void moveToTail(
            MAP&   prodFiles,
            ENTRY& prodEntry)
    {
        removeFromList<MAP, ENTRY>(prodFiles, prodEntry);
        addToTail<MAP, ENTRY>(prodFiles, prodEntry);
    }

    template <class MAP, class ENTRY>
    void erase(
            MAP&            prodFiles,
            const ProdIndex prodIndex)
    {
        ENTRY& prodEntry = prodFiles[headIndex];

        if (!prodEntry.isComplete())
            LOG_WARN("Closing incomplete product-file \"%s\" to reuse "
                    "file-descriptor");

        removeFromList<MAP, ENTRY>(prodFiles, prodEntry);
        prodFiles.erase(prodIndex); // Destructor closes file
    }

    template <class MAP, class ENTRY>
    ENTRY* getProdEntry(
            MAP&            prodFiles,
            const ProdIndex prodIndex)
    {
        ENTRY* prodEntry;
        auto   iter = prodFiles.find(prodIndex);

        if (iter == prodFiles.end()) {
            prodEntry = nullptr;
        }
        else {
            prodEntry = &iter->second;

            if (prodIndex != tailIndex)
                moveToTail<MAP,ENTRY>(prodFiles, *prodEntry);
        }

        return prodEntry;
    }

    /**
     * Adds a product-entry.
     *
     * @param[in] prodIndex    Product index
     * @param[in] prodSize     Product size in bytes
     * @return                 Pointer to product-entry
     * @throws    LogicError   Entry already exists
     * @throws    SystemError  Couldn't construct product-file
     */
    template <class MAP, class ENTRY, class PFILE>
    ENTRY* addProdEntry(
            MAP&                                           prodFiles,
            const ProdIndex                                prodIndex,
            const std::function<PFILE(const std::string&)> PfileCtor)
    {
        const std::string indexPath = getPathname(prodIndex);
        PFILE             prodFile;

        ensureDir(dirPath(indexPath), 0700);

        for (;;) {
            try {
                prodFile = PfileCtor(indexPath);
            }
            catch (const SystemError& ex) {
                const int errnum = ex.code().value();
                if ((errnum != EMFILE && errnum != ENFILE) || !headIndex)
                    throw;
                erase<MAP, ENTRY>(prodFiles, headIndex); // Destructor closes file
                continue;
            }
            break;
        }

        auto pair = prodFiles.emplace(std::piecewise_construct,
                std::forward_as_tuple(prodIndex),
                std::forward_as_tuple(prodIndex, prodFile));

        if (!pair.second)
            throw LOGIC_ERROR("Entry for product " + prodIndex.to_string() +
                    " already exists");

        return &pair.first->second;
    }

    template <class MAP, class ENTRY, class PFILE>
    ENTRY& getProdEntry(
            MAP&                                           prodFiles,
            const ProdIndex                                prodIndex,
            const std::function<PFILE(const std::string&)> PfileCtor)
    {
        ENTRY* prodEntry;
        auto   iter = prodFiles.find(prodIndex);

        if (iter == prodFiles.end()) {
            prodEntry = addProdEntry<MAP, ENTRY, PFILE>(prodFiles, prodIndex,
                    PfileCtor);
            addToTail<MAP, ENTRY>(prodFiles, *prodEntry);
        }
        else {
            prodEntry = &iter->second;

            if (prodIndex != tailIndex)
                moveToTail<MAP, ENTRY>(prodFiles, *prodEntry);
        }

        return *prodEntry;
    }

    void link(
            const std::string extantPath,
            const std::string newPath)
    {
        ensureDir(dirPath(newPath), 0700);

        if (::link(extantPath.data(), newPath.data()))
            throw SYSTEM_ERROR("Couldn't link \"" + newPath + "\" to \"" +
                    extantPath + "\"");
    }

    template <class MAP, class ENTRY>
    ProdInfo getProdInfo(
            MAP&            prodFiles,
            const ProdIndex prodIndex)
    {
        Guard  guard{mutex};
        ENTRY* prodEntry = getProdEntry<MAP,ENTRY>(prodFiles, prodIndex);

        return prodEntry
                ? ProdInfo(prodIndex, prodEntry->prodFile.getProdSize(),
                        prodEntry->namePath)
                : ProdInfo();
    }

    template <class MAP, class ENTRY, class PFILE>
    MemSeg getMemSeg(
            MAP&         prodFiles,
            const SegId& segId)
    {
        Guard  guard{mutex};
        ENTRY* prodEntry = getProdEntry<MAP,ENTRY>(prodFiles,
                segId.getProdIndex());

        if (!prodEntry)
            return MemSeg();

        PFILE&    prodFile = prodEntry->prodFile;
        ProdSize  prodSize = prodFile.getProdSize();
        ProdSize  offset = segId.getSegOffset();
        SegSize   segSize = prodFile.getSegSize(offset);
        SegInfo   segInfo(segId, prodSize, segSize);

        return MemSeg(segInfo, prodFile.getData(offset));
    }

    /**
     * Performs cleanup actions. Closes product-files that haven't been accessed
     * in 24 hours.
     *
     * @threadsafety       Safe
     * @exceptionsafety    Basic guarantee
     * @cancellationpoint  Yes
     */
    template <class MAP, class ENTRY>
    void cleanup(MAP& prodFiles)
    {
        Guard guard{mutex};

        while (auto prodIndex = headIndex)
            if (time(nullptr) -
                    getProdEntry<MAP, ENTRY>(prodFiles, prodIndex)->when >
                    86400)
                erase<MAP, ENTRY>(prodFiles, prodIndex);
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize)
        : rootPathname{rootPathname}
        , indexesDir{rootPathname + "/indexes"} // Index paths have '/' prefix
        , namesDir{rootPathname + "/names/"} // Name paths don't have '/' prefix
        , segSize{segSize}
        , mutex{}
        , headIndex{}
        , tailIndex{}
    {}

    virtual ~Impl() noexcept
    {}

    const std::string& getRootDir() const noexcept
    {
        return rootPathname;
    }

    const std::string& getNamesDir() const noexcept
    {
        return namesDir;
    }

    std::string getPathname(const ProdIndex prodIndex) const
    {
        return indexesDir + getIndexPath(prodIndex);
    }

    std::string getPathname(const std::string name) const
    {
        return namesDir + name;
    }
};

/******************************************************************************/

Repository::Repository(Impl* impl)
    : pImpl{impl}
{}

const std::string& Repository::getRootDir() const noexcept
{
    return pImpl->getRootDir();
}

const std::string& Repository::getNamesDir() const noexcept
{
    return pImpl->getNamesDir();
}

std::string Repository::getPathname(const ProdIndex prodId) const
{
    return pImpl->getPathname(prodId);
}

std::string Repository::getPathname(const std::string name) const
{
    return pImpl->getPathname(name);
}

/******************************************************************************/
/******************************************************************************/

typedef struct SrcProdEntry : ProdEntry
{
    SndProdFile prodFile; ///< File of product to be sent

    SrcProdEntry()
        : ProdEntry()
        , prodFile()
    {}

    SrcProdEntry(
            const ProdIndex     prodIndex,
            struct SndProdFile& prodFile)
        : ProdEntry(prodIndex)
        , prodFile(prodFile)
    {}

    SrcProdEntry& operator =(const SrcProdEntry& rhs)
    {
        ProdEntry::operator =(rhs);
        prodFile = rhs.prodFile;
        return *this;
    }

    bool isComplete() const
    {
        return true;
    }
} SrcProdEntry;

class SrcRepo::Impl final : public Repository::Impl
{
    typedef SndProdFile                          PFILE;
    typedef SrcProdEntry                         ENTRY;
    typedef std::unordered_map<ProdIndex, ENTRY> MAP;

    MAP prodFiles;

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize)
        : Repository::Impl{rootPathname, segSize}
    {}

    /**
     * Accepts notification of a new product-file in the repository's directory
     * hierarchy for named products.
     *
     * @param[in] prodName     Name of product
     * @param[in] prodIndex    Product index
     * @throws    LogicError   Entry for product already exists
     * @throws    SystemError  Couldn't construct product-file
     */
    void newProd(
            const std::string& prodName,
            const ProdIndex    prodIndex)
    {
        const std::string namePath = getPathname(prodName);

        link(namePath, getPathname(prodIndex));
        Repository::Impl::addProdEntry<MAP,ENTRY,PFILE>(prodFiles, prodIndex,
                [=](const std::string& indexPath) {
                    return SndProdFile(indexPath, segSize);
                });
    }

    ProdInfo getProdInfo(const ProdIndex prodIndex)
    {
        return Repository::Impl::getProdInfo<MAP,ENTRY>(prodFiles, prodIndex);
    }

    MemSeg getMemSeg(const SegId& segId)
    {
        return Repository::Impl::getMemSeg<MAP,ENTRY,PFILE>(prodFiles, segId);
    }
};

/******************************************************************************/

SrcRepo::SrcRepo(
        const std::string& rootPathname,
        const SegSize      segSize)
    : Repository{new Impl(rootPathname, segSize)}
{}

void SrcRepo::newProd(
        const std::string& prodName,
        const ProdIndex    prodIndex)
{
    static_cast<SrcRepo::Impl*>(pImpl.get())->newProd(prodName, prodIndex);
}

ProdInfo SrcRepo::getProdInfo(const ProdIndex prodIndex) const
{
    return static_cast<SrcRepo::Impl*>(pImpl.get())->getProdInfo(prodIndex);
}

MemSeg SrcRepo::getMemSeg(const SegId& segId) const
{
    return static_cast<SrcRepo::Impl*>(pImpl.get())->getMemSeg(segId);
}

/******************************************************************************/
/******************************************************************************/

typedef struct SnkProdEntry : ProdEntry
{
    RcvProdFile prodFile;     ///< File of product being received
    bool        haveNamePath; ///< Have pathname based on product-name?

    SnkProdEntry()
        : ProdEntry()
        , prodFile{}
        , haveNamePath{false}
    {}

    SnkProdEntry(
            const ProdIndex     prodIndex,
            struct RcvProdFile& prodFile)
        : ProdEntry(prodIndex)
        , prodFile{prodFile}
        , haveNamePath{false}
    {}

    SnkProdEntry& operator =(const SnkProdEntry& rhs)
    {
        ProdEntry::operator =(rhs);
        prodFile = rhs.prodFile;
        haveNamePath = rhs.haveNamePath;
        return *this;
    }

    bool isComplete() const
    {
        return haveNamePath && prodFile.isComplete();
    }
} SnkProdEntry;

class SnkRepo::Impl final : public Repository::Impl
{
    typedef RcvProdFile                          PFILE;
    typedef SnkProdEntry                         ENTRY;
    typedef std::unordered_map<ProdIndex, ENTRY> MAP;

    MAP prodFiles;

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize)
        : Repository::Impl{rootPathname, segSize}
    {}

    void save(const ProdInfo& prodInfo)
    {
        Lock          lock{mutex};
        SnkProdEntry& prodEntry = getProdEntry<MAP,ENTRY,PFILE>(prodFiles,
                prodInfo.getProdIndex(), [&](const std::string& indexPath) {
                    return RcvProdFile(indexPath, prodInfo.getSize(), segSize);
                });

        if (!prodEntry.haveNamePath) {
            prodEntry.namePath = prodInfo.getName();
            prodEntry.haveNamePath = true;
        }

        if (prodEntry.isComplete()) {
            const std::string indexPath{prodEntry.prodFile.getPathname()};
            const std::string namePath{getPathname(prodEntry.namePath)};

            lock.unlock();
            link(indexPath, namePath);
        }
    }

    void save(DataSeg& dataSeg)
    {
        Lock           lock{mutex};
        const SegInfo& segInfo = dataSeg.getSegInfo();
        SnkProdEntry&  prodEntry = getProdEntry<MAP,ENTRY,PFILE>(prodFiles,
                segInfo.getProdIndex(), [&](const std::string& indexPath) {
                    return RcvProdFile(indexPath, segInfo.getProdSize(),
                            segSize);
                });

        prodEntry.prodFile.accept(dataSeg);

        if (prodEntry.isComplete()) {
            const std::string indexPath{prodEntry.prodFile.getPathname()};
            const std::string namePath{getPathname(prodEntry.namePath)};

            lock.unlock();
            link(indexPath, namePath);
        }
    }

    ProdInfo getProdInfo(const ProdIndex prodIndex)
    {
        return Repository::Impl::getProdInfo<MAP,ENTRY>(prodFiles, prodIndex);
    }

    MemSeg getMemSeg(const SegId& segId)
    {
        return Repository::Impl::getMemSeg<MAP,ENTRY,PFILE>(prodFiles, segId);
    }
};

/******************************************************************************/

SnkRepo::SnkRepo(
        const std::string& rootPathname,
        const SegSize      segSize)
    : Repository{new Impl(rootPathname, segSize)}
{}

void SnkRepo::save(const ProdInfo& prodInfo) const
{
    static_cast<SnkRepo::Impl*>(pImpl.get())->save(prodInfo);
}

void SnkRepo::save(DataSeg& dataSeg) const
{
    static_cast<SnkRepo::Impl*>(pImpl.get())->save(dataSeg);
}

ProdInfo SnkRepo::getProdInfo(const ProdIndex prodIndex) const
{
    return static_cast<SnkRepo::Impl*>(pImpl.get())->getProdInfo(prodIndex);
}

MemSeg SnkRepo::getMemSeg(const SegId& segId) const
{
    return static_cast<SnkRepo::Impl*>(pImpl.get())->getMemSeg(segId);
}

} // namespace
