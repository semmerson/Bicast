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
 * Abstract product-file implementation
 */
class ProdFile::Impl
{
protected:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;

    mutable Mutex     mutex;
    const std::string pathname;
    char*             data; ///< For `get()`
    const ProdSize    prodSize;
    const ProdSize    numSegs;
    const int         fd;
    const SegSize     segSize;
    const SegSize     lastSegSize;

    /**
     * Returns the size of a file in bytes.
     *
     * @param[in] fd            File descriptor that's open on file
     * @return                  Size of file in bytes
     * @throws    SYSTEM_ERROR  `stat()` failure
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static ProdSize getFileSize(const int fd)
    {
        int         status;
        struct stat statBuf;
        {
            // Because `fstat()` can be a cancellation point
            Canceler canceler{false};
            status = ::fstat(fd, &statBuf);
        }
        if (status)
            throw SYSTEM_ERROR("stat() failure");
        return statBuf.st_size;
    }

    /**
     * Constructs.
     *
     * @param[in] pathname         Pathname of file
     * @param[in] segSize          Size of a canonical segment in bytes. Shall
     *                             not be zero if file has positive size.
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @param[in] fd               File descriptor open on the file
     * @cancellationpoint          No
     */
    Impl(   const std::string& pathname,
            const SegSize      segSize,
            const int          fd)
        : mutex()
        , pathname{pathname}
        , data{nullptr}
        , prodSize{getFileSize(fd)}
        , numSegs{segSize ? ((prodSize + segSize - 1) / segSize) : 0}
        , fd{fd}
        , segSize{segSize}
        , lastSegSize{static_cast<SegSize>(segSize
                ? (prodSize%segSize ? prodSize%segSize : segSize)
                : 0)}
    {
        if (prodSize && segSize == 0) {
            ::close(fd);
            throw INVALID_ARGUMENT("Zero segment-size specified for "
                    "non-empty file \"" + pathname + "\"");
        }
    }

    inline ProdSize segIndex(const ProdSize offset) const
    {
        return offset / segSize;
    }

    inline SegSize segLen(const ProdSize offset) const
    {
        return segIndex(offset) + 1 < numSegs
                ? segSize
                : lastSegSize;
    }

public:
    virtual ~Impl() noexcept =0;

    const std::string& getPathname() const noexcept
    {
        return pathname;
    }

    ProdSize getProdSize() const noexcept
    {
        return prodSize;
    }

    SegSize getSegSize(const ProdSize offset) const
    {
        vet(offset);
        return (offset + segSize > prodSize)
                ? prodSize - offset
                : segSize;
    }

    void vet(const ProdSize offset) const
    {
        // Following order works for zero segment-size; fails otherwise
        if ((offset >= prodSize) || (offset % segSize))
            throw INVALID_ARGUMENT("Invalid offset: {offset: " +
                    std::to_string(offset) + ", segSize: " +
                    std::to_string(segSize) + ", prodSize: " +
                    std::to_string(prodSize) + "}");
    }

    /**
     * This default implementation merely verifies the offset.
     *
     * @param[in] offset  Segment offset in bytes
     * @retval    `false`  Segment does not exist
     * @retval    `true`   Segment does exist
     */
    virtual bool exists(const ProdSize offset) const
    {
        vet(offset);
        return true;
    }

    const void* getData(const ProdSize offset) const
    {
        if (!exists(offset))
            throw INVALID_ARGUMENT("Segment at offset " + std::to_string(offset)
                    + " doesn't exist");

        return data + offset;
    }
};

ProdFile::Impl::~Impl() noexcept
{
    if (data)
        ::munmap(data, prodSize);
    ::close(fd);
}

/******************************************************************************/

ProdFile::ProdFile(Impl* impl)
    : pImpl{impl}
{}

ProdFile::~ProdFile() noexcept
{}

ProdFile::operator bool() noexcept
{
    return static_cast<bool>(pImpl);
}

const std::string& ProdFile::getPathname() const noexcept
{
    return pImpl->getPathname();
}

ProdSize ProdFile::getProdSize() const noexcept
{
    return pImpl->getProdSize();
}

SegSize ProdFile::getSegSize(const ProdSize offset) const
{
    return pImpl->getSegSize(offset);
}

void ProdFile::vet(const ProdSize offset)
{
    return pImpl->vet(offset);
}

const void* ProdFile::getData(const ProdSize offset) const
{
    return pImpl->getData(offset);
}

/******************************************************************************/
/******************************************************************************/

/**
 * Product-file implementation for the source of data-products.
 */
class SndProdFile::Impl final : public ProdFile::Impl
{
    /**
     * Opens an existing file.
     *
     * @param[in] pathname      Pathname of file
     * @return                  File descriptor that's open on file
     * @throws    SYSTEM_ERROR  `open()` failure
     */
    static int open(const std::string& pathname)
    {
        int fd = ::open(pathname.data(), O_RDONLY);
        if (fd == -1)
            throw SYSTEM_ERROR("open() failure on \"" + pathname + "\"");
        return fd;
    }

public:
    Impl(   const std::string& pathname,
            const SegSize      segSize)
        : ProdFile::Impl{pathname, segSize, open(pathname)}
    {
        if (prodSize) {
            data = static_cast<char*>(::mmap(static_cast<void*>(0), prodSize,
                    PROT_READ, MAP_PRIVATE, fd, 0));
            if (data == MAP_FAILED) {
                ::close(fd);
                throw SYSTEM_ERROR("mmap() failure: {pathname: \"" + pathname +
                        "\", prodSize: " + std::to_string(prodSize) + ", fd: " +
                        std::to_string(fd) + "}");
            } // Memory-mapping failed
        } // Positive product-size
    }

    bool exists(const ProdSize offset)
    {
        vet(offset);
        return true;
    }
};

/******************************************************************************/

SndProdFile::SndProdFile()
    : ProdFile{nullptr}
{}

SndProdFile::SndProdFile(
        const std::string& pathname,
        const SegSize      segSize)
    : ProdFile{new Impl(pathname, segSize)}
{}

bool SndProdFile::exists(ProdSize offset) const
{
    return static_cast<SndProdFile::Impl*>(pImpl.get())->exists(offset);
}

/******************************************************************************/
/******************************************************************************/

/**
 * Product-file implementation for receivers of data-products.
 */
class RcvProdFile::Impl final : public ProdFile::Impl
{
    std::vector<bool> haveSegs;
    ProdSize          segCount;

    /**
     * Creates a file. The file will have the given size and be zero-filled.
     *
     * @param[in] pathname      Pathname of file
     * @param[in] prodSize      Size of file in bytes
     * @param[in] segSize       Size of canonical segment in bytes
     * @return                  File descriptor that's open on file
     * @throws    SYSTEM_ERROR  `open()` or `ftruncate()` failure
     */
    static int create(
            const std::string& pathname,
            const ProdSize     prodSize,
            const SegSize      segSize)
    {
        if (prodSize && segSize == 0)
            throw INVALID_ARGUMENT("Zero segment-size specified for non-empty "
                    "file \"" + pathname + "\"");

        ensureDir(dirPath(pathname), 0700);

        const int fd = ::open(pathname.data(), O_RDWR|O_CREAT, 0600);
        if (fd == -1)
            throw SYSTEM_ERROR("open() failure on \"" + pathname + "\"");

        try {
            int status = ::ftruncate(fd, prodSize);
            if (status)
                throw SYSTEM_ERROR("ftruncate() failure on \"" + pathname +
                        "\"");
        }
        catch (...) {
            ::close(fd);
            ::unlink(pathname.data());
            throw;
        }

        return fd;
    }

public:
    Impl(   const std::string& pathname,
            const ProdSize     prodSize,
            const SegSize      segSize)
        : ProdFile::Impl{pathname, segSize, create(pathname, prodSize, segSize)}
        , haveSegs(numSegs, false)
        , segCount{0}
    {
        if (prodSize) {
            data = static_cast<char*>(::mmap(static_cast<void*>(0), prodSize,
                    PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0));
            if (data == MAP_FAILED) {
                ::close(fd);
                ::unlink(pathname.data());
                throw SYSTEM_ERROR("mmap() failure: {pathname: \"" + pathname +
                        "\", prodSize: " + std::to_string(prodSize) + ", fd: " +
                        std::to_string(fd) + "}");
            } // Memory-mapping failed
        } // Positive product-size
    }

    bool exists(const ProdSize offset) const
    {
        vet(offset);
        Guard guard(mutex);
        return haveSegs[segIndex(offset)];
    }

    bool accept(DataSeg& seg)
    {
        const ProdSize offset = seg.getOffset();
        vet(offset);

        const auto segInfo = seg.getSegInfo();
        const auto segSize = segInfo.getSegSize();
        const auto expectSize = segLen(offset);
        if (segSize != segLen(offset))
            throw INVALID_ARGUMENT("Segment " + segInfo.to_string() + " should "
                    "have " + std::to_string(expectSize) + " data-bytes");

        ProdSize   iSeg = segIndex(offset);
        Guard      guard(mutex);
        const bool accepted = !haveSegs[iSeg];

        if (accepted) {
            seg.getData(data+offset); // Potentially slow
            ++segCount;
            haveSegs[iSeg] = true;
        }

        return accepted;
    }

    bool isComplete() const
    {
        Guard guard(mutex);
        return segCount == numSegs;
    }
};

/******************************************************************************/

RcvProdFile::RcvProdFile()
    : ProdFile{nullptr}
{}

RcvProdFile::RcvProdFile(
        const std::string& pathname,
        const ProdSize     prodSize,
        const SegSize      segSize)
    : ProdFile{new Impl(pathname, prodSize, segSize)}
{}

bool
RcvProdFile::exists(const ProdSize offset) const
{
    return static_cast<Impl*>(pImpl.get())->exists(offset);
}

bool
RcvProdFile::save(DataSeg& seg) const
{
    return static_cast<Impl*>(pImpl.get())->accept(seg);
}

bool RcvProdFile::isComplete() const
{
    return static_cast<Impl*>(pImpl.get())->isComplete();
}

} // namespace
