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

class SndProdEntry : public ProdEntry<SndProdFile>
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

class RcvProdEntry : public ProdEntry<RcvProdFile>
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

template <class PE>
class ProdFiles
{
    typedef std::unordered_map<ProdIndex,PE> Map;

    Map       prodFiles; ///< Product files
    ProdIndex headIndex; ///< Index of product at head of queue
    ProdIndex tailIndex; ///< Index of product at tail of queue

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
    ProdFiles()
        : prodFiles{}
        , headIndex{}
        , tailIndex{}
    {}

    PE* add(PE& prodEntry)
    {
        auto  pair = prodFiles.emplace(std::piecewise_construct,
                std::forward_as_tuple(prodEntry.getProdInfo().getProdIndex()),
                std::forward_as_tuple(prodEntry));

        if (pair.second)
            addToTail(pair.first->second);

        return &pair.first->second;
    }

    PE* find(const ProdIndex prodIndex)
    {
        auto  iter = prodFiles.find(prodIndex);

        if (iter == prodFiles.end())
            return nullptr;

        PE& prodEntry = iter->second;
        moveToTail(prodEntry);

        return &prodEntry;
    }

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
 * @tparam PF  Product-file type
 */
class Repository::Impl
{
protected:
    typedef std::mutex                          Mutex;
    typedef std::lock_guard<Mutex>              Guard;
    typedef std::unique_lock<Mutex>             Lock;

    mutable Mutex     mutex;        ///< For concurrency control
    const std::string rootPathname; ///< Pathname of root of repository

    /// Pathname of root of directory hierarchy of product files based on
    /// product-indexes
    const std::string indexesDir;

    /// Pathname of root of directory hierarchy of product files based on
    /// product-names
    const std::string namesDir;

    const SegSize     segSize;      ///< Size of canonical data-segment in bytes

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

    static void link(
            const std::string& extantPath,
            const std::string& newPath)
    {
        ensureDir(dirPath(newPath), 0700);

        if (::link(extantPath.data(), newPath.data()))
            throw SYSTEM_ERROR("Couldn't link \"" + newPath + "\" to \"" +
                    extantPath + "\"");
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize)
        : rootPathname{rootPathname}
        , indexesDir{rootPathname + "/indexes"} // Index paths have '/' prefix
        , namesDir{rootPathname + "/names/"} // Name paths don't have '/' prefix
        , segSize{segSize}
        , mutex{}
    {}

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

    const std::string& getNamesDir() const noexcept
    {
        return namesDir;
    }

    std::string getPathname(const ProdIndex prodIndex) const
    {
        return indexesDir + getIndexPath(prodIndex);
    }

    std::string getPathname(const std::string& name) const
    {
        return namesDir + name;
    }

    std::string getProdName(const std::string& namePath) const
    {
        const auto prefixLen = namesDir.length();
        return namePath.substr(prefixLen, namePath.length()-prefixLen);
    }

    virtual bool exists(const ProdIndex prodIndex) =0;

    virtual bool exists(const SegId& segId) =0;

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
: pImpl{impl}
{}

SegSize Repository::getSegSize() const noexcept
{
    return pImpl->getSegSize();
}

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

class SndRepo::Impl final : public Repository::Impl
{
    ProdFiles<SndProdEntry> prodFiles; ///< Product files

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize)
        : Repository::Impl{rootPathname, segSize}
    {}

    /**
     * Accepts notification of a new product-file in the repository's directory
     * for named products.
     *
     * @param[in] pathname     Pathname of product-file
     * @param[in] prodIndex    Product index
     * @throws    LogicError   Entry for product already exists
     * @throws    SystemError  Couldn't open product-file
     */
    void newProd(
            const std::string& prodName,
            const ProdIndex    prodIndex)
    {
        Guard      guard{mutex};
        const auto entry = prodFiles.find(prodIndex);

        if (entry)
            throw LOGIC_ERROR("Entry already exists for file \"" + prodName +
                    "\"");

        const auto namePath = getPathname(prodName);
        const auto indexPath = getPathname(prodIndex);

        link(namePath, indexPath);

        SndProdFile  prodFile(indexPath, segSize);
        SndProdEntry prodEntry{prodName, prodIndex, prodFile};
        prodFiles.add(prodEntry);
    }

    bool exists(const ProdIndex prodIndex)
    {
        Guard guard{mutex};
        return prodFiles.find(prodIndex) != nullptr;
    }

    bool exists(const SegId& segId)
    {
        Guard guard{mutex};
        const auto prodEntry = prodFiles.find(segId.getProdIndex());

        return prodEntry && prodEntry->exists(segId.getOffset());
    }

    ProdInfo getProdInfo(const ProdIndex prodIndex)
    {
        Guard      guard{mutex};
        const auto prodEntry = prodFiles.find(prodIndex);

        return (prodEntry == nullptr)
                ? ProdInfo{}
                : prodEntry->getProdInfo();
    }

    MemSeg getMemSeg(const SegId& segId)
    {
        Guard      guard{mutex};
        const auto prodEntry = prodFiles.find(segId.getProdIndex());

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

SndRepo::SndRepo(
        const std::string& rootPathname,
        const SegSize      segSize)
    : Repository{new Impl(rootPathname, segSize)}
{}

void SndRepo::newProd(
        const std::string& prodName,
        const ProdIndex    prodIndex)
{
    static_cast<SndRepo::Impl*>(pImpl.get())->newProd(prodName, prodIndex);
}

bool SndRepo::exists(const ProdIndex prodIndex) const
{
    return static_cast<SndRepo::Impl*>(pImpl.get())->exists(prodIndex);
}

bool SndRepo::exists(const SegId& segId) const
{
    return static_cast<SndRepo::Impl*>(pImpl.get())->exists(segId);
}

ProdInfo SndRepo::getProdInfo(const ProdIndex prodIndex) const
{
    return static_cast<SndRepo::Impl*>(pImpl.get())->getProdInfo(prodIndex);
}

MemSeg SndRepo::getMemSeg(const SegId& segId) const
{
    return static_cast<SndRepo::Impl*>(pImpl.get())->getMemSeg(segId);
}

/******************************************************************************/
/******************************************************************************/

class RcvRepo::Impl final : public Repository::Impl
{
    ProdFiles<RcvProdEntry> prodFiles; ///< Product files
    RcvRepoObs&             repoObs;   ///< Observer of this instance

    void link(RcvProdEntry* entry)
    {
        const auto indexPath = entry->getProdFile().getPathname();
        const auto namePath = getPathname(entry->getProdName());

        Repository::Impl::link(indexPath, namePath);
    }

public:
    Impl(   const std::string& rootPathname,
            const SegSize      segSize,
            RcvRepoObs&        repoObs)
        : Repository::Impl{rootPathname, segSize}
        , repoObs(repoObs)
    {}

    bool save(const ProdInfo& prodInfo)
    {
        Lock       lock{mutex};
        bool       wasSaved;
        const auto prodIndex = prodInfo.getProdIndex();
        auto       entry = prodFiles.find(prodIndex);

        if (entry == nullptr) {
            LOG_DEBUG("Saving product-information %s",
                    prodInfo.to_string().data());
            RcvProdFile  prodFile{getPathname(prodIndex),
                prodInfo.getProdSize(), segSize};
            RcvProdEntry prodEntry{prodInfo, prodFile};

            entry = prodFiles.add(prodEntry);
            wasSaved = true;
        }
        else if (entry->getProdName().empty()) {
            LOG_DEBUG("Saving product-information %s",
                    prodInfo.to_string().data());
            entry->setProdName(prodInfo.getProdName());
            wasSaved = true;
        }
        else {
            LOG_DEBUG("Ignoring product-information %s",
                    prodInfo.to_string().data());
            wasSaved = false;
        }

        if (wasSaved && entry->isComplete()) {
            link(entry);
            repoObs.completed(prodInfo);
        }

        return wasSaved;
    }

    bool save(DataSeg& dataSeg)
    {
        Guard           guard{mutex};
        bool            wasSaved;
        const auto      segInfo = dataSeg.getSegInfo();
        const ProdIndex prodIndex = segInfo.getProdIndex();
        auto            entry = prodFiles.find(prodIndex);

        if (entry == nullptr) {
            LOG_DEBUG("Saving data-segment %s", dataSeg.to_string().data());
            const auto     indexPath = getPathname(prodIndex);
            const auto     prodSize = segInfo.getProdSize();
            const ProdInfo prodInfo(prodIndex, prodSize, "");
            RcvProdFile    prodFile{indexPath, prodSize, segSize};
            RcvProdEntry   prodEntry{prodIndex, prodFile};

            entry = prodFiles.add(prodEntry);
            entry->getProdFile().save(dataSeg);
            wasSaved = true;
        }
        else if (entry->exists(segInfo.getSegId().getOffset())) {
            LOG_DEBUG("Ignoring data-segment %s", dataSeg.to_string().data());
            wasSaved = false;
        }
        else {
            LOG_DEBUG("Saving data-segment %s", dataSeg.to_string().data());
            entry->getProdFile().save(dataSeg);
            wasSaved = true;

            if (entry->isComplete()) {
                link(entry);
                repoObs.completed(entry->getProdInfo());
            }
        }

        return wasSaved;
    }

    ProdInfo getProdInfo(const ProdIndex prodIndex)
    {
        Guard      guard{mutex};
        const auto prodEntry = prodFiles.find(prodIndex);

        return (prodEntry == nullptr || prodEntry->getProdName().empty())
                ? ProdInfo{}
                : prodEntry->getProdInfo();
    }

    MemSeg getMemSeg(const SegId& segId)
    {
        Guard      guard{mutex};
        const auto prodEntry = prodFiles.find(segId.getProdIndex());

        if (prodEntry == nullptr)
            return MemSeg{};

        auto prodFile = prodEntry->getProdFile();
        auto offset = segId.getOffset();

        return MemSeg(SegInfo(segId, prodFile.getProdSize(),
                    prodFile.getSegSize(offset)),
                    prodFile.getData(offset));
    }

    bool exists(const ProdIndex prodIndex)
    {
        Guard      guard{mutex};
        const auto prodEntry = prodFiles.find(prodIndex);

        return prodEntry && !prodEntry->getProdName().empty();
    }

    bool exists(const SegId& segId)
    {
        Guard      guard{mutex};
        const auto prodEntry = prodFiles.find(segId.getProdIndex());

        return prodEntry && prodEntry->exists(segId.getOffset());
    }
};

/******************************************************************************/

RcvRepo::RcvRepo(
        const std::string& rootPathname,
        const SegSize      segSize,
        RcvRepoObs&        repoObs)
    : Repository{new Impl(rootPathname, segSize, repoObs)}
{}

bool RcvRepo::save(const ProdInfo& prodInfo) const
{
    return static_cast<RcvRepo::Impl*>(pImpl.get())->save(prodInfo);
}

bool RcvRepo::save(DataSeg& dataSeg) const
{
    return static_cast<RcvRepo::Impl*>(pImpl.get())->save(dataSeg);
}

ProdInfo RcvRepo::getProdInfo(const ProdIndex prodIndex) const
{
    return static_cast<RcvRepo::Impl*>(pImpl.get())->getProdInfo(prodIndex);
}

MemSeg RcvRepo::getMemSeg(const SegId& segId) const
{
    return static_cast<RcvRepo::Impl*>(pImpl.get())->getMemSeg(segId);
}

bool RcvRepo::exists(const ProdIndex prodIndex) const
{
    return static_cast<RcvRepo::Impl*>(pImpl.get())->exists(prodIndex);
}

bool RcvRepo::exists(const SegId& segId) const
{
    return static_cast<RcvRepo::Impl*>(pImpl.get())->exists(segId);
}

} // namespace
