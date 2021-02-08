/**
 * A thread-safe file that contains a data-product.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: ProdFile.cpp
 *  Created on: Dec 17, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "ProdFile.h"

#include "error.h"
#include "FileUtil.h"
#include "Thread.h"

#include <fcntl.h>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hycast {

/**
 * Abstract, base product-file implementation
 */
class ProdFile::Impl
{
    void map(const int prot) {
        if (prodSize) {
            data = static_cast<char*>(::mmap(static_cast<void*>(0), prodSize,
                    prot, MAP_SHARED, fd, 0));
            if (data == MAP_FAILED) {
                throw SYSTEM_ERROR("mmap() failure: {pathname: \"" + pathname +
                        "\", prodSize: " + std::to_string(prodSize) + ", fd: " +
                        std::to_string(fd) + "}");
            } // Memory-mapping failed
        } // Positive product-size
    }

    void unmap() {
        ::munmap(data, prodSize);
    }

protected:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;

    mutable Mutex     mutex;
    std::string       pathname;
    char*             data; ///< For `get()`
    const ProdSize    prodSize;
    const ProdSize    numSegs;
    int               fd;
    const SegSize     segSize;
    const SegSize     lastSegSize;

    /**
     * Opens an existing file.
     *
     * @param[in] rootFd        File descriptor open on root directory
     * @param[in] pathname      Pathname of file
     * @param[in] mode          Open mode
     * @return                  File descriptor that's open on file
     * @throws    SYSTEM_ERROR  `open()` failure
     */
    static int open(
            const int          rootFd,
            const std::string& pathname,
            const int          mode)
    {
        int fd = ::openat(rootFd, pathname.data(), mode, 0600);
        if (fd == -1)
            throw SYSTEM_ERROR("open() failure on \"" + pathname + "\"");
        return fd;
    }

    /**
     * Returns the size of a file in bytes.
     *
     * @param[in] rootFd        File descriptor open on root-directory of
     *                          repository
     * @param[in] pathname      Pathname of existing file
     * @return                  Size of file in bytes
     * @throws    SYSTEM_ERROR  `stat()` failure
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static ProdSize getFileSize(
            const int          rootFd,
            const std::string& pathname)
    {
        const int fd = open(rootFd, pathname, O_RDONLY);
        if (fd == -1)
            throw SYSTEM_ERROR("Couldn't open file \"" + pathname + "\"");

        try {
            struct stat statBuf;
            Canceler    canceler{false}; // Because `fstat()` can be cancellation point
            int         status = ::fstat(fd, &statBuf);

            if (status)
                throw SYSTEM_ERROR("stat() failure");

            ::close(fd);
            return statBuf.st_size;
        } // `fd` is open
        catch (...) {
            ::close(fd);
            throw;
        }
    }

    /**
     * Constructs. The instance is closed.
     *
     * @param[in] pathname         Pathname of file
     * @param[in] prodSize         Size of file in bytes
     * @param[in] segSize          Size of a canonical segment in bytes. Shall
     *                             not be zero if file has positive size.
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @cancellationpoint          No
     */
    Impl(   const std::string& pathname,
            const ProdSize     prodSize,
            const SegSize      segSize)
        : mutex()
        , pathname{pathname}
        , data{nullptr}
        , prodSize{prodSize}
        , numSegs{segSize ? ((prodSize + segSize - 1) / segSize) : 0}
        , fd{-1}
        , segSize{segSize}
        , lastSegSize{static_cast<SegSize>(segSize
                ? (prodSize%segSize ? prodSize%segSize : segSize)
                : 0)}
    {
        if (prodSize && segSize == 0)
            throw INVALID_ARGUMENT("Zero segment-size specified for "
                    "non-empty file \"" + pathname + "\"");
    }

    /**
     * Ensures access to the underlying file. Opens the file if necessary. Maps
     * the file if necessary. Idempotent.
     *
     * @param[in] rootFd       File descriptor open on the root directory
     * @param[in] mode         File open mode
     * @throws    SystemError  Couldn't open file
     */
    void ensureAccess(
            const int rootFd,
            const int mode) {
        const bool wasClosed = fd < 0;

        if (wasClosed) {
            fd = open(rootFd, pathname, mode);
            if (fd == -1)
                throw SYSTEM_ERROR("Couldn't open file \"" + pathname + "\"");
        }

        try {
            if (data == nullptr) {
                const int prot = (mode == O_RDONLY)
                        ? PROT_READ
                        : PROT_READ|PROT_WRITE;
                map(prot);
            }
        } // `fd` is open
        catch (const std::exception& ex) {
            if (wasClosed) {
                ::close(fd);
                fd = -1;
            }
            throw;
        }
    }

    /**
     * Disables access to the data of the underlying file. Idempotent.
     */
    void disableAccess() noexcept {
        if (data) {
            unmap();
            data = nullptr;
        }

        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

    inline ProdSize segIndex(const ProdSize offset) const {
        return offset / segSize;
    }

    inline SegSize segLen(const ProdSize offset) const {
        return segIndex(offset) + 1 < numSegs
                ? segSize
                : lastSegSize;
    }

    /**
     * Vets a data-segment.
     *
     * @param[in] offset       Offset of data-segment in bytes
     * @throws    LOGIC_ERROR  Offset is greater than product's size or isn't an
     *                         integral multiple of canonical segment size
     */
    void vet(const ProdSize offset) const {
        // Following order works for zero segment-size
        if ((offset >= prodSize) || (offset % segSize))
            throw INVALID_ARGUMENT("Invalid offset: {offset: " +
                    std::to_string(offset) + ", segSize: " +
                    std::to_string(segSize) + ", prodSize: " +
                    std::to_string(prodSize) + "}");
    }

public:
    virtual ~Impl() noexcept {
        disableAccess();
    }

    const std::string& getPathname() const noexcept {
        return pathname;
    }

    ProdSize getProdSize() const noexcept {
        return prodSize;
    }

    SegSize getSegSize(const ProdSize offset) const {
        vet(offset);
        return (offset + segSize > prodSize)
                ? prodSize - offset
                : segSize;
    }

    virtual void open(const int rootFd) =0;

    void close() {
        Guard guard(mutex);
        disableAccess();
    }

    /**
     * Indicates if a data-segment exists.
     *
     * @param[in] offset  Segment offset in bytes
     * @retval    `false`  Segment does not exist
     * @retval    `true`   Segment does exist
     */
    virtual bool exists(const ProdSize offset) const =0;

    const void* getData(const ProdSize offset) const {
        if (!exists(offset))
            throw INVALID_ARGUMENT("Segment at offset " + std::to_string(offset)
                    + " doesn't exist");

        return data + offset;
    }
};

/******************************************************************************/

ProdFile::ProdFile() noexcept =default;

ProdFile::ProdFile(std::shared_ptr<Impl>&& pImpl) noexcept
    : pImpl(pImpl) {
}

ProdFile::ProdFile(const ProdFile& prodFile) noexcept =default;

ProdFile::ProdFile(ProdFile&& prodFile) noexcept =default;

ProdFile::~ProdFile() =default;

ProdFile& ProdFile::operator =(const ProdFile& rhs) noexcept =default;

ProdFile& ProdFile::operator =(ProdFile&& rhs) noexcept =default;

ProdFile::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

const std::string& ProdFile::getPathname() const noexcept {
    return pImpl->getPathname();
}

ProdSize ProdFile::getProdSize() const noexcept {
    return pImpl->getProdSize();
}

SegSize ProdFile::getSegSize(const ProdSize offset) const {
    return pImpl->getSegSize(offset);
}

const void* ProdFile::getData(const ProdSize offset) const {
    return pImpl->getData(offset);
}

void ProdFile::close() const {
    return pImpl->close();
}

/******************************************************************************/
/******************************************************************************/

/**
 * Product-file implementation for the publisher of data-products.
 */
class SndProdFile::Impl final : public ProdFile::Impl
{
public:
    /**
     * Constructs. The instance is open.
     *
     * @param[in] rootFd    File descriptor open on root-directory of repository
     * @param[in] pathname  Pathname of the file
     * @param[in] segSize   Size of a canonical data-segment in bytes
     */
    Impl(   const int          rootFd,
            const std::string& pathname,
            const SegSize      segSize)
        : ProdFile::Impl{pathname, getFileSize(rootFd, pathname), segSize}
    {
        ensureAccess(rootFd, O_RDONLY);
    }

    void open(const int rootFd) override {
        Guard guard(mutex);
        ensureAccess(rootFd, O_RDONLY);
    }

    bool exists(const ProdSize offset) const override {
        vet(offset);
        return true;
    }
};

/******************************************************************************/

SndProdFile::SndProdFile() noexcept =default;

SndProdFile::SndProdFile(
        const int          rootFd,
        const std::string& pathname,
        const SegSize      segSize)
    : ProdFile(std::make_shared<Impl>(rootFd, pathname, segSize)) {
}

void SndProdFile::open(const int rootFd) const {
    static_cast<SndProdFile::Impl*>(pImpl.get())->open(rootFd);
}

bool SndProdFile::exists(ProdSize offset) const {
    return static_cast<SndProdFile::Impl*>(pImpl.get())->exists(offset);
}

/******************************************************************************/
/******************************************************************************/

/**
 * Product-file implementation for receivers of data-products.
 */
class RcvProdFile::Impl final : public ProdFile::Impl
{
    ProdIndex         prodIndex;  ///< Product index
    std::vector<bool> haveSegs;   ///< Bitmap of set data-segments
    ProdSize          segCount;   ///< Number of set data-segments
    bool              pathIsName; ///< File pathname is product name?

    /**
     * Creates a file from product-information. The file will have the given
     * size and be zero-filled.
     *
     * @param[in] rootFd        File descriptor open on root directory
     * @param[in] prodInfo      Product-information
     * @param[in] segSize       Size of canonical segment in bytes
     * @return                  File descriptor on open file
     * @throws    SYSTEM_ERROR  `open()` or `ftruncate()` failure
     */
    static int create(
            const int          rootFd,
            const std::string& pathname,
            const ProdSize&    prodSize)
    {
        ensureDir(rootFd, dirPath(pathname), 0700);

        const int fd = ProdFile::Impl::open(rootFd, pathname,
                O_RDWR|O_CREAT|O_EXCL);
        if (fd == -1)
            throw SYSTEM_ERROR("Couldn't create file \"" + pathname + "\"");

        try {
            if (::ftruncate(fd, prodSize))
                throw SYSTEM_ERROR("ftruncate() failure on \"" + pathname +
                        "\"");
            return fd;
        } // `fd` is open
        catch (...) {
            ::close(fd);
            ::unlink(pathname.data());
            throw;
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

public:
    /**
     * Constructs. The instance is open.
     *
     * @param[in] rootFd           File descriptor open on root directory of
     *                             repository
     * @param[in] prodIndex        Product index
     * @param[in] prodSize         Product size in bytes
     * @param[in] segSize          Canonical segment size in bytes
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @throws    SystemError      `open()` or `ftruncate()` failure
     */
    Impl(   const int       rootFd,
            const ProdIndex prodIndex,
            const ProdSize  prodSize,
            const SegSize   segSize)
        : ProdFile::Impl{prodIndex.to_string(), prodSize, segSize}
        , prodIndex(prodIndex)
        , haveSegs(numSegs, false)
        , segCount{0}
        , pathIsName(false)
    {
        fd = create(rootFd, pathname, prodSize);
        ensureAccess(rootFd, O_RDWR);
    }

    void open(const int rootFd) override {
        Guard guard(mutex);
        ensureAccess(rootFd, O_RDWR);
    }

    bool exists(const ProdSize offset) const {
        vet(offset);
        Guard guard(mutex);
        return haveSegs[segIndex(offset)];
    }

    /**
     * Indicates if the product is complete.
     *
     * @pre             State is locked
     * @retval `true`   Product is complete
     * @retval `false`  Product is not complete
     */
    bool isComplete() const
    {
        Guard guard{mutex};
        return pathIsName && (segCount == numSegs);
    }

    /**
     * Saves product-information.
     *
     * @param[in] rootFd       File descriptor open on repository's root
     *                         directory
     * @param[in] prodInfo     Product information to be saved
     * @retval    `true`       This item was written to the product-file
     * @retval    `false`      This item is already in the product-file and was
     *                         not written
     * @throws    SystemError  Couldn't save product information
     */
    bool save(
            const int       rootFd,
            const ProdInfo& prodInfo) {
        assert(fd >= 0);

        bool  wasSaved; // This item was written to the product-file?
        Guard guard(mutex);

        ProdInfo expected(prodIndex, prodSize, prodInfo.getProdName());
        if (!(prodInfo == expected))
            throw INVALID_ARGUMENT("Actual prodInfo, " + prodInfo.to_string() +
                    ", doesn't match expected, " + expected.to_string());

        if (pathIsName) {
            wasSaved = false;
        }
        else {
            LOG_DEBUG("Saving product-information " + prodInfo.to_string());

            const auto prodName = prodInfo.getProdName();

            ensureDir(rootFd, dirPath(prodName), 0755); // Only owner can write

            if (::renameat(rootFd, this->pathname.data(), rootFd,
                    prodName.data()))
                throw SYSTEM_ERROR("Couldn't rename product-file \"" +
                        this->pathname + "\" to \"" + prodName + "\"");

            this->pathname = prodName;
            pathIsName = true;
            wasSaved = true;
        }

        return wasSaved;
    }

    /**
     * Saves a data-segment.
     *
     * @pre                        Instance is open
     * @param[in] seg              Data-segment to be saved
     * @retval    `true`           This item was written to the product-file
     * @retval    `false`          This item is already in the product-file and
     *                             was not written
     * @throws    InvalidArgument  Segment is invalid
     */
    bool save(DataSeg& seg) {
        assert(fd >= 0);

        const ProdSize offset = seg.getSegOffset();
        vet(offset);

        const auto segInfo = seg.getSegInfo();
        const auto segSize = segInfo.getSegSize();
        const auto expectSize = segLen(offset);
        if (segSize != segLen(offset))
            throw INVALID_ARGUMENT("Segment " + segInfo.to_string() + " should "
                    "have " + std::to_string(expectSize) + " data-bytes");

        ProdSize iSeg = segIndex(offset);
        bool     wasSaved; // This item was written to the product-file?
        {
            Guard guard(mutex);

            wasSaved = !haveSegs[iSeg];

            if (!wasSaved) {
                LOG_WARN("Duplicate data segment: " + seg.to_string());
            }
            else {
                haveSegs[iSeg] = true;
            }
        }

        if (wasSaved) {
            // Setting data outside mutex supports concurrent data-setting
            LOG_DEBUG("Saving data-segment " + seg.getSegId().to_string());

            seg.getData(data+offset); // Potentially slow
            {
                Guard guard(mutex);
                ++segCount;
            }
        }
        else {
            char buf[segSize];
            seg.getData(buf);
        }

        return wasSaved;
    }

    ProdInfo getProdInfo() {
        return ProdInfo(prodIndex, prodSize, pathname);
    }
};

/******************************************************************************/

RcvProdFile::RcvProdFile() noexcept =default;

RcvProdFile::RcvProdFile(
        const int       rootFd,
        const ProdIndex prodIndex,
        const ProdSize  prodSize,
        const SegSize   segSize)
    : ProdFile(std::make_shared<Impl>(rootFd, prodIndex, prodSize, segSize)) {
}

void RcvProdFile::open(const int rootFd) const {
    return static_cast<Impl*>(pImpl.get())->open(rootFd);
}

bool
RcvProdFile::exists(const ProdSize offset) const {
    return static_cast<Impl*>(pImpl.get())->exists(offset);
}

bool RcvProdFile::isComplete() const {
    return static_cast<Impl*>(pImpl.get())->isComplete();
}

bool
RcvProdFile::save(
        const int       rootFd,
        const ProdInfo& prodInfo) const {
    return static_cast<Impl*>(pImpl.get())->save(rootFd, prodInfo);
}

bool
RcvProdFile::save(DataSeg& dataSeg) const {
    return static_cast<Impl*>(pImpl.get())->save(dataSeg);
}

ProdInfo
RcvProdFile::getProdInfo() const {
    return static_cast<Impl*>(pImpl.get())->getProdInfo();
}

} // namespace
