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

class DataSeg::Impl {
public:
    DataSegId   segId;    ///< Data-segment identifier
    /// Product size in bytes (for when product notice is missed)
    ProdSize    prodSize;
    const char* buf;

    Impl(const DataSegId& segId,
         const ProdSize   prodSize,
         const char*      data)
        : segId(segId)
        , prodSize(prodSize)
        , buf(data)
    {}

    virtual ~Impl() {}

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
};

class SockSeg final : public DataSeg::Impl
{
    char buf[DataSeg::CANON_DATASEG_SIZE];

public:
    SockSeg(const DataSegId& segId,
                     const ProdSize   prodSize,
                     TcpSock&         sock)
        : Impl(segId, prodSize, buf)
    {
        if (!sock.read(buf, DataSeg::size(prodSize, segId.offset)))
            throw EOF_ERROR("EOF encountered reading data-segment " +
                    segId.to_string());
    }
};

/******************************************************************************/

DataSeg::DataSeg()
    : pImpl{}
{}

DataSeg::DataSeg(const DataSegId& segId,
        const ProdSize   prodSize,
        const char*      data)
    : pImpl(std::make_shared<Impl>(segId, prodSize, data))
{}

DataSeg::DataSeg(const DataSegId& segId,
                 const ProdSize   prodSize,
                 TcpSock&         sock)
    : pImpl(std::make_shared<SockSeg>(segId, prodSize, sock))
{}

DataSeg::operator bool() const {
    return static_cast<bool>(pImpl);
}

const DataSegId& DataSeg::segId() const noexcept {
    return pImpl->segId;
}

ProdSize DataSeg::prodSize() const noexcept {
    return pImpl->prodSize;
}

const char* DataSeg::data() const noexcept {
    return pImpl->buf;
}

String DataSeg::to_string(const bool withName) const {
    return pImpl->to_string(withName);
}

} // namespace
