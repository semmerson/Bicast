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

#include <memory>

namespace hycast {

class DataSeg::Impl
{
public:
    DataSegId   segId;    ///< Data-segment identifier
    /// Product size in bytes (for when product notice is missed)
    ProdSize    prodSize;
    SegSize     bufSize;  ///< Size of buffer in bytes
    char*       buf;      ///< buffer for data

    Impl()
        : segId()
        , prodSize(0)
        , bufSize(0)
        , buf(nullptr)
    {}

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

    operator bool() const noexcept {
        return buf != nullptr;
    }

    const char* data() const noexcept {
        return buf;
    }

    String to_string(const bool withName) const {
        String string;
        if (withName)
            string += "DataSeg";
        return string + "{segId=" + segId.to_string() + ", prodSize=" +
                std::to_string(prodSize) + "}";
    }

    bool write(Xprt& xprt) {
        LOG_NOTE("Writing data-segment to %s", xprt.to_string().data());
        auto success = segId.write(xprt);
        if (success) {
            LOG_NOTE("Writing product-size to %s", xprt.to_string().data());
            success = xprt.write(prodSize);
            if (success) {
                LOG_NOTE("Writing data-segment data to %s",
                        xprt.to_string().data());
                success = xprt.write(buf, DataSeg::size(prodSize, segId.offset));
            }
        }
        return success;
    }

    bool read(Xprt& xprt) {
        LOG_NOTE("Reading data-segment from %s", xprt.to_string().data());
        bool success = segId.read(xprt);

        if (success) {
            LOG_NOTE("Reading product-size from %s", xprt.to_string().data());
            success = xprt.read(prodSize);

            if (success) {
                auto segSize = DataSeg::size(prodSize, segId.offset);
                if (bufSize < segSize) {
                    delete[] buf;
                    buf = new char[segSize];
                    bufSize = segSize;
                }
                LOG_NOTE("Reading data-segment data from %s",
                        xprt.to_string().data());
                success = xprt.read(buf, segSize);
            }
            if (success) {
                LOG_NOTE("Read data-segment");
            }
            else {
                LOG_NOTE("Didn't read data-segment");
            }
        }

        return success;
    }
};

/******************************************************************************/

DataSeg::DataSeg()
    : pImpl(new Impl())
{}

DataSeg::DataSeg(const DataSegId segId,
                 const ProdSize  prodSize,
                 const char*     data)
    : pImpl(new Impl(segId, prodSize, data))
{}

DataSeg::operator bool() const {
    return pImpl->operator bool();
}

const DataSegId DataSeg::getId() const noexcept {
    return pImpl->segId;
}

ProdSize DataSeg::getProdSize() const noexcept {
    return pImpl->prodSize;
}

const char* DataSeg::getData() const noexcept {
    return pImpl->buf;
}

String DataSeg::to_string(const bool withName) const {
    return pImpl->to_string(withName);
}

bool DataSeg::write(Xprt& xprt) const {
    return pImpl->write(xprt);
}

bool DataSeg::read(Xprt& xprt) {
    return pImpl->read(xprt);
}

} // namespace
