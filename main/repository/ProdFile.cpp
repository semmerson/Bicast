/**
 * A thread-safe file that contains a data-product.
 *
 *        File: ProdFile.cpp
 *  Created on: Dec 17, 2019
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

#include "ProdFile.h"

#include "error.h"
#include "FileUtil.h"
#include "Shield.h"

#include <fcntl.h>
#include <mutex>
#include <chrono>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace hycast {

/**
 * Base product-file implementation. Good for the source of data-products.
 */
class ProdFile::Impl
{
    const ProdSize  prodSize;
    int             mode; ///< Open mode (e.g., O_RDONLY, O_RDWR)
    const SegSize   segSize;
    const SegSize   lastSegSize;
    const Timestamp deleteTime;

    /**
     * Opens the underlying file. Idempotent.
     *
     * @param[in] rootFd        File descriptor open on root directory
     * @throws    SYSTEM_ERROR  `open()` failure
     */
    void openFile(const int rootFd) {
        if (fd < 0) {
            fd = ::openat(rootFd, pathname.data(), mode, 0600);
            if (fd == -1)
                throw SYSTEM_ERROR("open() failure on \"" + pathname + "\"");
        }
    }

    void mapFile(const int prot) {
        if (prodSize) {
            data = static_cast<char*>(::mmap(static_cast<void*>(0), prodSize, prot, MAP_SHARED, fd,
                    0));
            if (data == MAP_FAILED) {
                throw SYSTEM_ERROR("mmap() failure: {pathname: \"" + pathname +
                        "\", prodSize: " + std::to_string(prodSize) + ", fd: " +
                        std::to_string(fd) + "}");
            } // Memory-mapping failed
        } // Positive product-size
    }

    void unmapFile() {
        ::munmap(data, prodSize);
    }

    /**
     * Ensures access to the underlying file. Opens the file if necessary. Maps the file if
     * necessary. Idempotent.
     *
     * @param[in] rootFd       File descriptor open on the root directory
     * @param[in] mode         File open mode
     * @throws    SystemError  Couldn't open file
     */
    void ensureAccess(const int rootFd) {
        const bool wasClosed = fd < 0;

        if (wasClosed) {
            openFile(rootFd);
            if (fd == -1)
                throw SYSTEM_ERROR("Couldn't open file \"" + pathname + "\"");
        }

        try {
            if (data == nullptr)
                mapFile((mode == O_RDONLY) ? PROT_READ : PROT_READ|PROT_WRITE);
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
            unmapFile();
            data = nullptr;
        }

        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

protected:
    mutable Mutex     mutex;
    String            pathname;
    char*             data; ///< For `get()`
    const ProdSize    numSegs;
    int               fd;

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
    /**
     * Returns the size of a file in bytes.
     *
     * @param[in] rootFd        File descriptor open on root-directory of product-files
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
        const int fd = ::openat(rootFd, pathname.data(), O_RDONLY);
        if (fd == -1)
            throw SYSTEM_ERROR("Couldn't open file \"" + pathname + "\"");

        try {
            struct stat statBuf;
            Shield      shield; // Because `fstat()` can be cancellation point
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
     * Constructs. The instance is not open.
     *
     * @param[in] pathname         Pathname of file relative to root directory
     * @param[in] prodSize         Size of file in bytes
     * @param[in] segSize          Size of a canonical segment in bytes. Shall not be zero if file
     *                             has positive size.
     * @param[in] mode             Open mode (e.g., O_RDONLY, O_RDWR)
     * @param[in] deleteTime       When this product should be deleted
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @cancellationpoint          No
     */
    Impl(   const std::string& pathname,
            const SegSize      segSize,
            const ProdSize     prodSize,
            const int          mode,
            const Timestamp&   deleteTime)
        : prodSize{prodSize}
        , mode(mode)
        , segSize{segSize}
        , lastSegSize{static_cast<SegSize>(segSize
                ? (prodSize%segSize ? prodSize%segSize : segSize)
                : 0)}
        , deleteTime(deleteTime)
        , mutex()
        , pathname(pathname)
        , data{nullptr}
        , numSegs{segSize ? ((prodSize + segSize - 1) / segSize) : 0}
        , fd{-1}
    {
        if (prodSize && segSize == 0)
            throw INVALID_ARGUMENT("Zero segment-size specified for non-empty file \"" + pathname +
                    "\"");
    }

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

    const Timestamp& getDeleteTime() const {
        return deleteTime;
    }

    String to_string() const {
        return "{pathname=" + pathname + ", prodSize=" + std::to_string(prodSize) + ", fd=" +
                std::to_string(fd) + ", deleteTime=" + deleteTime.to_string() + "}";
    }

    void open(const int rootFd) {
        Guard guard(mutex);
        ensureAccess(rootFd);
    }

    void close() {
        Guard guard(mutex);
        disableAccess();
    }

    void deleteFile() {
        Guard guard(mutex);
        disableAccess();
        if (::unlinkat(fd, pathname.data(), 0))
            throw SYSTEM_ERROR("Couldn't delete file \"" + pathname + "\"");
    }

    const char* getData(const ProdSize offset) const {
        vet(offset);
        Guard guard{mutex};
        if (data == nullptr)
            throw LOGIC_ERROR("Product file + " + to_string() + " isn't open");
        return data + offset;
    }

    /**
     * The following, default functions are valid for a publisher's product-files: they are not
     * valid for a subscriber's.
     */

    virtual bool exists(const ProdSize offset) const {
        vet(offset);
        return true;
    }

    virtual bool isComplete() const {
        return true;
    }

    virtual bool save(const DataSeg& dataSeg) {
        throw LOGIC_ERROR("Operation not supported by this instance");
    }

    virtual void rename(
            const int      rootFd,
            const String&  pathname) {
        throw LOGIC_ERROR("Operation not supported by this instance");
    }
};

/**************************************************************************************************/

ProdFile::ProdFile(Impl* pImpl) noexcept
    : pImpl(pImpl) {
}

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

const char* ProdFile::getData(const ProdSize offset) const {
    return pImpl->getData(offset);
}

const Timestamp& ProdFile::getDeleteTime() const {
    return pImpl->getDeleteTime();
}

void ProdFile::close() const {
    return pImpl->close();
}

void ProdFile::deleteFile() const {
    return pImpl->deleteFile();
}

void ProdFile::open(const int rootFd) const {
    pImpl->open(rootFd);
}

bool ProdFile::exists(const ProdSize offset) const {
    return pImpl->exists(offset);
}

bool ProdFile::isComplete() const {
    return pImpl->isComplete();
}

bool ProdFile::save(const DataSeg& dataSeg) const {
    return pImpl->save(dataSeg);
}

void ProdFile::rename(
        const int      rootFd,
        const String&  pathname) const {
    pImpl->rename(rootFd, pathname);
}

/**************************************************************************************************/
/**************************************************************************************************/

/**
 * Product-file implementation for a publisher of data-products.
 */
class SendProdFile final : public ProdFile::Impl
{
public:
    /**
     * Constructs. The instance is open.
     *
     * @param[in] rootFd      File descriptor open on the root-directory of the product-files
     * @param[in] pathname    Pathname of the underlying file relative to the root-directory
     * @param[in] segSize     Maximum data-segment size in bytes
     * @param[in] deleteTime  When this product should be deleted
     */
    SendProdFile(
            const int        rootFd,
            const String&    pathname,
            const SegSize    segSize,
            const Timestamp& deleteTime)
        : ProdFile::Impl{pathname, segSize, ProdFile::Impl::getFileSize(rootFd, pathname), O_RDWR,
                deleteTime}
    {
        open(rootFd);
    }
};

ProdFile::ProdFile(
        const int        rootFd,
        const String&    pathname,
        const SegSize    segSize,
        const Timestamp& deleteTime)
    : ProdFile(new SendProdFile(rootFd, pathname, segSize, deleteTime))
{}

/**************************************************************************************************/
/**************************************************************************************************/

/**
 * Product-file implementation for a subscriber of data-products.
 */
class RecvProdFile final : public ProdFile::Impl
{
    std::vector<bool> haveSegs;   ///< Bitmap of received data-segments
    ProdSize          segCount;   ///< Number of received data-segments

public:
    /**
     * Constructs. Creates a new, underlying file from product-information. The file will have the
     * given size and be zero-filled. The instance is open.
     *
     * @param[in] rootFd           File descriptor open on root directory of product-files
     * @param[in] pathname         Pathname of the file
     * @param[in] segSize          Canonical segment size in bytes
     * @param[in] prodSize         Product size in bytes
     * @param[in] deleteTime       When this product should be deleted
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @throws    SystemError      `::open()` or `::ftruncate()` failure
     */
    RecvProdFile(
            const int        rootFd,
            const String&    pathname,
            const SegSize    segSize,
            const ProdSize   prodSize,
            const Timestamp& deleteTime)
        : ProdFile::Impl(pathname, segSize, prodSize, O_RDWR, deleteTime)
        , haveSegs(numSegs, false)
        , segCount{0}
    {
        ensureDir(rootFd, dirname(pathname), 0700);

        fd = ::openat(rootFd, pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0600);
        if (fd == -1)
            throw SYSTEM_ERROR("Couldn't create file \"" + pathname + "\"");

        try {
            if (::ftruncate(fd, prodSize))
                throw SYSTEM_ERROR("ftruncate() failure on \"" + pathname + "\"");
            open(rootFd);
        } // `fd` is open
        catch (...) {
            ::close(fd);
            ::unlink(pathname.data());
            throw;
        }
    }

    bool exists(const ProdSize offset) const override {
        vet(offset);
        Guard guard(mutex);
        return haveSegs[segIndex(offset)];
    }

    /**
     * Saves a data-segment.
     *
     * @pre                        Instance is open
     * @param[in] seg              Data-segment to be saved
     * @retval    `true`           This item is new and was saved
     * @retval    `false`          This item is old and was not saved
     * @throws    LogicError       Instance is not open
     * @throws    InvalidArgument  Segment is invalid
     * @see `open()`
     */
    bool save(const DataSeg& seg) override {
        if (fd < 0 || data == nullptr)
            throw LOGIC_ERROR("Instance is not open");

        const ProdSize offset = seg.getOffset();
        vet(offset);

        const auto segSize = seg.getSize();
        const auto expectSize = segLen(offset);
        if (segSize != expectSize)
            throw INVALID_ARGUMENT("Segment " + seg.getId().to_string() + " has " +
                    std::to_string(segSize) + " bytes; not " + std::to_string(expectSize));

        ProdSize iSeg = segIndex(offset);
        bool     needed; // This item was written to the product-file?
        {
            Guard guard(mutex);
            needed = !haveSegs[iSeg];

            if (!needed) {
                LOG_DEBUG("Duplicate data segment: " + seg.getId().to_string());
            }
            else {
                //LOG_DEBUG("Saving data-segment " + seg.getId().to_string());
                haveSegs[iSeg] = true;
                ::memcpy(data+offset, seg.getData(), segSize);
                ++segCount;
            }
        }

        return needed;
    }

    /**
     * Indicates if the product is complete.
     *
     * @pre             State is locked
     * @retval `true`   Product is complete
     * @retval `false`  Product is not complete
     */
    bool isComplete() const override {
        Guard guard{mutex};
        return segCount == numSegs;
    }

    void rename(
            const int     rootFd,
            const String& pathname) override {
        ensureDir(rootFd, dirname(pathname), 0755); // Only owner can write

        Guard guard(mutex);
        if (::renameat(rootFd, this->pathname.data(), rootFd, pathname.data()))
            throw SYSTEM_ERROR("Couldn't rename product-file \"" + this->pathname + "\" to \"" +
                    pathname + "\"");
        this->pathname = pathname;
    }
};

/**************************************************************************************************/

ProdFile::ProdFile(
        const int        rootFd,
        const String&    pathname,
        const SegSize    segSize,
        const ProdSize   prodSize,
        const Timestamp& deleteTime)
    : ProdFile(new RecvProdFile(rootFd, pathname, segSize, prodSize, deleteTime))
{}

} // namespace
