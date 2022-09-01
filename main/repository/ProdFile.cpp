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
#include <sys/stat.h>
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
private:
    int mode; ///< Open mode (e.g., O_RDONLY, O_RDWR)

    void mapFile(const int prot) {
        data = static_cast<char*>(::mmap(static_cast<void*>(0), prodSize, prot, MAP_SHARED, fd,
                0));
        if (data == MAP_FAILED) {
            throw SYSTEM_ERROR("mmap() failure: {pathname: \"" + pathname +
                    "\", prodSize: " + std::to_string(prodSize) + ", fd: " +
                    std::to_string(fd) + "}");
        } // Memory-mapping failed
    }

    void unmapFile() {
        ::munmap(data, prodSize);
    }

protected:
    mutable Mutex     mutex;
    const ProdSize    prodSize;
    String            pathname;
    char*             data; ///< For `get()`
    const ProdSize    numSegs;
    int               fd;

    /**
     * Ensures access to the underlying file. Opens the file if necessary. Maps the file if
     * necessary. Idempotent.
     *
     * @pre                    Instance is locked
     * @param[in] mode         File open mode
     * @throws    SystemError  Couldn't open file
     * @post                   Instance is locked
     */
    void ensureAccess() {
        LOG_ASSERT(!mutex.try_lock());

        if (fd < 0) {
            fd = ::open(pathname.data(), mode, 0600);
            if (fd == -1)
                throw SYSTEM_ERROR("open() failure on \"" + pathname + "\"");

            try {
                mapFile((mode == O_RDONLY) ? PROT_READ : PROT_READ|PROT_WRITE);
            } // `fd` is open
            catch (const std::exception& ex) {
                ::close(fd);
                fd = -1;
                throw;
            }
        }
    }

    /**
     * Disables access to the data of the underlying file. Idempotent.
     *
     * @pre                    Instance is locked
     * @post                   Instance is locked
     */
    void disableAccess() noexcept {
        LOG_ASSERT(!mutex.try_lock());

        if (fd >= 0) {
            unmapFile();
            data = nullptr;

            ::close(fd);
            fd = -1;
        }
    }

    /**
     * Vets a data-segment.
     *
     * @param[in] offset           Offset of data-segment in bytes
     * @throws    InvalidArgument  Offset is greater than product's size or isn't an integral
     *                             multiple of maximum segment size
     */
    void vet(const ProdSize offset) const {
        // Following order works for zero segment-size
        if ((offset >= prodSize) || (offset % DataSeg::getMaxSegSize()))
            throw INVALID_ARGUMENT("Invalid offset: {offset: " +
                    std::to_string(offset) + ", segSize: " +
                    std::to_string(DataSeg::getMaxSegSize()) + ", prodSize: " +
                    std::to_string(prodSize) + "}");
    }

public:
    /**
     * Constructs. The instance is not open.
     *
     * @param[in] pathname         Pathname of file relative
     * @param[in] prodSize         Size of file in bytes
     * @param[in] mode             Open mode (e.g., O_RDONLY, O_RDWR)
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @cancellationpoint          No
     */
    Impl(   const std::string& pathname,
            const ProdSize     prodSize,
            const int          mode)
        : mode(mode)
        , mutex()
        , prodSize{prodSize}
        , pathname(pathname)
        , data{nullptr}
        , numSegs{DataSeg::numSegs(prodSize)}
        , fd{-1}
    {}

    virtual ~Impl() {
        Guard guard{mutex};
        disableAccess();
    }

    const std::string& getPathname() const noexcept {
        return pathname;
    }

    ProdSize getProdSize() const noexcept {
        return prodSize;
    }

    SysTimePoint getModTime() {
        SysTimePoint modTime;
        return FileUtil::getModTime(pathname, modTime);
    }

    SysTimePoint& getModTime(SysTimePoint& modTime) {
        return FileUtil::getModTime(pathname, modTime);
    }

    void setModTime(const SysTimePoint& modTime) {
        FileUtil::setModTime(pathname, modTime, false);
    }

    String to_string() const {
        return "{pathname=" + pathname + ", prodSize=" + std::to_string(prodSize) + ", fd=" +
                std::to_string(fd) + "}";
    }

    void close() {
        Guard guard(mutex);
        disableAccess();
    }

    /**
     * NB: An open file can still be accessed until `close()` is called.
     */
    void deleteFile() {
        Guard guard(mutex);
        if (::unlink(pathname.data()))
            throw SYSTEM_ERROR("::unlink() failure on file file \"" + pathname + "\"");
    }

    const char* getData(const ProdSize offset) {
        vet(offset);
        Guard guard{mutex};
        ensureAccess();
        if (data == nullptr)
            throw LOGIC_ERROR("Product file " + to_string() + " isn't open");
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

    virtual void rename(const String& newPathname) {
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

String ProdFile::to_string() const {
    return static_cast<bool>(pImpl) ? pImpl->to_string() : "<unset>";
}

const std::string& ProdFile::getPathname() const noexcept {
    return pImpl->getPathname();
}

ProdSize ProdFile::getFileSize() const noexcept {
    return pImpl->getProdSize();
}

const char* ProdFile::getData(const ProdSize offset) const {
    return pImpl->getData(offset);
}

void ProdFile::close() const {
    return pImpl->close();
}

SysTimePoint ProdFile::getModTime() const {
    return pImpl->getModTime();
}

SysTimePoint& ProdFile::getModTime(SysTimePoint& modTime) const {
    return pImpl->getModTime(modTime);
}

void ProdFile::setModTime(const SysTimePoint& modTime) const {
    pImpl->setModTime(modTime);
}

void ProdFile::deleteFile() const {
    return pImpl->deleteFile();
}

bool ProdFile::exists(const ProdSize offset) const {
    return pImpl->exists(offset);
}

bool ProdFile::isComplete() const {
    return pImpl->isComplete();
}

void ProdFile::rename(const String& newPathname) const {
    return pImpl->rename(newPathname);
}

bool ProdFile::save(const DataSeg& dataSeg) const {
    return pImpl->save(dataSeg);
}

/**************************************************************************************************/
/**************************************************************************************************/

/**
 * Product-file implementation for a publisher of data-products.
 */
class PubProdFile final : public ProdFile::Impl
{
public:
    /**
     * Constructs. The instance is open.
     *
     * @param[in] pathname    Pathname of the underlying file
     */
    PubProdFile(const String& pathname)
        : ProdFile::Impl{pathname, static_cast<ProdSize>(FileUtil::getFileSize(pathname)), O_RDONLY}
    {}
};

ProdFile::ProdFile(const String& pathname)
    : ProdFile(new PubProdFile(pathname))
{}

/**************************************************************************************************/
/**************************************************************************************************/

/**
 * Product-file implementation for a subscriber of data-products.
 */
class SubProdFile final : public ProdFile::Impl
{
    std::vector<bool> haveSegs;      ///< Bitmap of received data-segments
    ProdSize          segCount;      ///< Number of received data-segments

public:
    /**
     * Constructs a subscriber's product-file. Creates a new, underlying file from
     * product-information and any necessary antecedent directories. The file will have the given
     * size and be zero-filled. The instance is not open.
     *
     * @param[in] pathname         Pathname of the file
     * @param[in] prodSize         Product size in bytes
     * @throws    InvalidArgument  `prodSize != 0 && segSize == 0`
     * @throws    SystemError      `::open()` or `::ftruncate()` failure
     */
    SubProdFile(
            const String&    pathname,
            const ProdSize   prodSize)
        : ProdFile::Impl(pathname, prodSize, O_RDWR)
        , haveSegs(numSegs, false)
        , segCount{0}
    {
        FileUtil::ensureDir(FileUtil::dirname(pathname), 0700);

        fd = ::open(pathname.data(), O_RDWR|O_CREAT, 0600);
        if (fd == -1)
            throw SYSTEM_ERROR("::open() failure on file \"" + pathname + "\"");

        try {
            if (::ftruncate(fd, prodSize))
                throw SYSTEM_ERROR("::ftruncate() failure on \"" + pathname + "\"");
            ::close(fd);
            fd = -1;
        } // `fd` is open
        catch (...) {
            ::close(fd);
            throw;
        }
    }

    bool exists(const ProdSize offset) const override {
        vet(offset);
        Guard guard(mutex);
        return haveSegs[DataSeg::getSegIndex(offset)];
    }

    /**
     * Saves a data-segment.
     *
     * @param[in] seg              Data-segment to be saved
     * @retval    `true`           This item is new and was saved
     * @retval    `false`          This item is old and was not saved
     * @throws    LogicError       Instance is not open
     * @throws    InvalidArgument  Segment's offset is greater than product's size or isn't an
     *                             integral multiple of maximum segment size
     * @throws    InvalidArgument  Segment size is invalid
     */
    bool save(const DataSeg& seg) override {
        const ProdSize offset = seg.getOffset();
        vet(offset);

        const auto segSize = seg.getSize();
        const auto expectSize = DataSeg::size(prodSize, offset);
        if (segSize != expectSize)
            throw INVALID_ARGUMENT("Segment sizes don't match: expect=" + std::to_string(expectSize)
                    + ", actual=" + std::to_string(segSize));

        ProdSize iSeg = DataSeg::getSegIndex(offset);
        Guard    guard(mutex);
        bool     needed = !haveSegs[iSeg];

        if (!needed) {
            LOG_DEBUG("Duplicate data segment: " + seg.getId().to_string());
        }
        else {
            //LOG_DEBUG("Saving data-segment " + seg.getId().to_string());
            ensureAccess();
            ::memcpy(data+offset, seg.getData(), segSize);
            haveSegs[iSeg] = true;
            ++segCount;
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

    void rename(const String& newPathname) override {
        Guard guard{mutex};
        if (::rename(pathname.data(), newPathname.data()))
            throw SYSTEM_ERROR("Couldn't rename file \"" + pathname + "\" to \"" + newPathname +
                    "\"");
        pathname = newPathname;
    }
};

/**************************************************************************************************/

ProdFile::ProdFile(
        const String&    pathname,
        const ProdSize   prodSize)
    : ProdFile(new SubProdFile(pathname, prodSize))
{}

} // namespace
