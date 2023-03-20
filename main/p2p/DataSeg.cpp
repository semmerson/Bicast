/**
 * This file implements a data segment
 *
 *  @file:  DataSeg.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#include "error.h"
#include "HycastProto.h"
#include "Socket.h"

#include <atomic>
#include <memory>

namespace hycast {

/// An implementation of a data segment
class DataSeg::Impl
{
    static std::atomic<SegSize> maxSegSize; ///< Maximum data-segment size in bytes

public:
    DataSegId   segId;    ///< Data-segment identifier
    /// Product size in bytes (for when product notice is missed)
    ProdSize    prodSize;
    SegSize     bufSize;  ///< Size of buffer in bytes
    char*       buf;      ///< buffer for data

    /**
     * Sets the maximum size, in bytes, of a data segment. All segments except the last will be this
     * size.
     * @param[in] maxSegSize  Maximum size of a data segment
     * @return                The previous maximum segment size
     */
    static SegSize setMaxSegSize(const SegSize maxSegSize) {
        if (maxSegSize <= 0)
            throw INVALID_ARGUMENT("Argument is not positive: " + std::to_string(maxSegSize));
        SegSize prev = Impl::maxSegSize;
        Impl::maxSegSize = maxSegSize;
        return prev;
    }

    /**
     * Returns the maximum size of a data segment.
     * @return The maximum size of a data segment
     */
    static SegSize getMaxSegSize() {
        return maxSegSize;
    }

    /**
     * Returns the expected size of a data segment.
     * @param[in] prodSize  Size of the containing data product in bytes
     * @param[in] offset    Offset to the start of this segment in the product in bytes
     * @return              The expected size of the data segment
     */
    static SegSize size(
            const ProdSize  prodSize,
            const SegOffset offset) noexcept {
        const auto nbytes = prodSize - offset;
        return (nbytes <= maxSegSize)
                ? nbytes
                : static_cast<SegSize>(maxSegSize);
    }

    /**
     * Returns the number of data segments in a product.
     * @param[in] prodSize  The size of the product in bytes
     * @return              The number of data segments in the product
     */
    static ProdSize numSegs(const ProdSize prodSize) noexcept {
        return (prodSize + (maxSegSize - 1)) / maxSegSize;
    }

    /**
     * Returns the origin-0 index of a data segment.
     * @param[in] offset  Offset, in bytes, of the start of a segment
     * @return            The origin-0 index of the data segment
     */
    static ProdSize getSegIndex(const ProdSize offset) {
        return offset/maxSegSize;
    }

    Impl()
        : segId()
        , prodSize(0)
        , bufSize(0)
        , buf(nullptr)
    {}

    /**
     * Constructs.
     * @param[in] segId     The segment's ID
     * @param[in] prodSize  The size of the containing product in bytes
     * @param[in] data      The segment's data
     */
    Impl(const DataSegId& segId,
         const ProdSize   prodSize,
         const char*      data)
        : segId(segId)
        , prodSize(prodSize)
        , bufSize(DataSeg::size(prodSize, segId.offset))
        , buf(new char[bufSize])
    {
        (void)::memcpy(buf, data, bufSize);
    }

    virtual ~Impl() noexcept {
        delete[] buf;
    }

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const noexcept {
        return buf != nullptr;
    }

    /**
     * Returns this segments data.
     * @return This segments data
     */
    const char* data() const noexcept {
        return buf;
    }

    /**
     * Tests for equality with another instance.
     * @param[in] rhs      The other, right-hand-side instance
     * @return    true     This instance is equal to the other instance
     * @return    false    This instance is not equal to the other instance
     */
    bool operator==(const Impl& rhs) {
        if (buf == nullptr && rhs.buf == nullptr)
            return true;
        if (buf == nullptr || rhs.buf == nullptr)
            return false;
        return (prodSize == rhs.prodSize) &&
                (segId == rhs.segId) &&
                (::memcmp(buf, rhs.buf, DataSeg::size(prodSize, segId.offset))
                        == 0);
    }

    /**
     * Returns the string representation of this instance.
     * @param[in] withName  Should the name of this class be included?
     * @return              The string representation of this instance
     */
    String to_string(const bool withName) const {
        String string;
        if (withName)
            string += "DataSeg";
        return string + "{segId=" + segId.to_string() + ", prodSize=" +
                std::to_string(prodSize) + "}";
    }

    /**
     * Writes itself to a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(Xprt xprt) {
        //LOG_DEBUG("Writing data-segment to %s", xprt.to_string().data());
        auto success = segId.write(xprt);
        if (success) {
            //LOG_DEBUG("Writing product-size to %s", xprt.to_string().data());
            success = xprt.write(prodSize);
            if (success) {
                //LOG_DEBUG("Writing data-segment data to %s",
                        //xprt.to_string().data());
                success = xprt.write(buf, DataSeg::size(prodSize, segId.offset));
            }
        }
        return success;
    }

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool read(Xprt xprt) {
        //LOG_DEBUG("Reading data-segment from %s", xprt.to_string().data());
        bool success = segId.read(xprt);

        if (success) {
            //LOG_DEBUG("Reading product-size from %s", xprt.to_string().data());
            success = xprt.read(prodSize);

            if (success) {
                auto segSize = DataSeg::size(prodSize, segId.offset);
                if (bufSize < segSize) {
                    delete[] buf;
                    buf = new char[segSize];
                    bufSize = segSize;
                }
                //LOG_DEBUG("Reading data-segment data from %s", xprt.to_string().data());
                success = xprt.read(buf, segSize);
            }
            if (success) {
                //LOG_DEBUG("Read data-segment");
            }
            else {
                //LOG_DEBUG("Didn't read data-segment");
            }
        }

        return success;
    }
};

std::atomic<SegSize> DataSeg::Impl::maxSegSize; ///< Maximum data-segment size in bytes

/******************************************************************************/

SegSize DataSeg::setMaxSegSize(const SegSize maxSegSize) noexcept {
    return Impl::setMaxSegSize(maxSegSize);
}

SegSize DataSeg::getMaxSegSize() noexcept {
    return Impl::getMaxSegSize();
}

SegSize DataSeg::size(
        const ProdSize  prodSize,
        const SegOffset offset) noexcept {
    return Impl::size(prodSize, offset);
}

ProdSize DataSeg::numSegs(const ProdSize prodSize) noexcept {
    return Impl::numSegs(prodSize);
}

ProdSize DataSeg::getSegIndex(const ProdSize offset) noexcept {
    return Impl::getSegIndex(offset);
}

DataSeg::DataSeg()
    : pImpl(new Impl())
{}

DataSeg::DataSeg(const DataSegId segId,
                 const ProdSize  prodSize,
                 const char*     data)
    : pImpl(new Impl(segId, prodSize, data))
{}

DataSeg::operator bool() const {
    return pImpl ? pImpl->operator bool() : false;
}

const DataSegId& DataSeg::getId() const noexcept {
    return pImpl->segId;
}

ProdSize DataSeg::getProdSize() const noexcept {
    return pImpl->prodSize;
}

const char* DataSeg::getData() const noexcept {
    return pImpl->buf;
}

bool DataSeg::operator==(const DataSeg& rhs) const {
    return !pImpl
            ? !rhs
            : rhs
                  ? *pImpl == *rhs.pImpl
                  : false;
}

String DataSeg::to_string(const bool withName) const {
    return pImpl->to_string(withName);
}

bool DataSeg::write(Xprt xprt) const {
    return pImpl->write(xprt);
}

bool DataSeg::read(Xprt xprt) {
    return pImpl->read(xprt);
}

} // namespace
